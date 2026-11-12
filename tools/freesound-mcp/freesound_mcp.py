# /// script
# requires-python = ">=3.10"
# dependencies = ["mcp>=1.2,<2", "httpx>=0.27"]
# ///
"""
MYE Freesound MCP — search, audition and download Freesound sounds from Claude.

Env vars:
  FREESOUND_API_KEY      API key (= "client secret" in your Freesound API credentials)
  FREESOUND_CLIENT_ID    Client ID (only needed to download originals via OAuth2)
  FREESOUND_DOWNLOAD_DIR Where sounds go (default: ~/Music/Freesound)
"""
from __future__ import annotations

import json
import os
import re
import time
from datetime import date
from pathlib import Path

import httpx
from mcp.server.fastmcp import FastMCP

API = "https://freesound.org/apiv2"
API_KEY = os.environ.get("FREESOUND_API_KEY", "")
CLIENT_ID = os.environ.get("FREESOUND_CLIENT_ID", "")
DOWNLOAD_DIR = Path(os.environ.get("FREESOUND_DOWNLOAD_DIR", "~/Music/Freesound")).expanduser()
TOKEN_FILE = Path("~/.config/mye-freesound/oauth.json").expanduser()

FIELDS = "id,name,username,duration,license,tags,previews,url,type,samplerate,channels,avg_rating,num_downloads,description"

LICENSE_FILTERS = {
    "any": "",
    "cc0": 'license:"Creative Commons 0"',
    "commercial_ok": '-license:"Attribution NonCommercial" -license:"Attribution Noncommercial" -license:"Sampling+"',
}

mcp = FastMCP("freesound")


# ---------- helpers ----------

def _client() -> httpx.Client:
    return httpx.Client(timeout=60, follow_redirects=True)


def _need_key() -> None:
    if not API_KEY:
        raise RuntimeError("Falta FREESOUND_API_KEY. Pedila en https://freesound.org/apiv2/apply y ponela en la config del MCP.")


def _get(path: str, params: dict) -> dict:
    _need_key()
    params = {k: v for k, v in params.items() if v not in (None, "")}
    params["token"] = API_KEY
    with _client() as c:
        r = c.get(f"{API}{path}", params=params)
        r.raise_for_status()
        return r.json()


def _short_license(lic: str) -> str:
    l = (lic or "").lower()
    if "/zero/" in l or "creative commons 0" in l:
        return "CC0 (dominio público)"
    if "by-nc" in l or "noncommercial" in l:
        return "CC BY-NC (atribución, NO comercial)"
    if "sampling+" in l:
        return "Sampling+ (restringida)"
    if "/by/" in l or "attribution" in l:
        return "CC BY (atribución)"
    return lic


def _summ(s: dict) -> dict:
    prev = s.get("previews") or {}
    return {
        "id": s.get("id"),
        "nombre": s.get("name"),
        "autor": s.get("username"),
        "duracion_s": round(s.get("duration") or 0, 2),
        "licencia": _short_license(s.get("license", "")),
        "formato": f'{s.get("type")} {s.get("samplerate")}Hz {s.get("channels")}ch',
        "rating": round(s.get("avg_rating") or 0, 1),
        "descargas": s.get("num_downloads"),
        "tags": (s.get("tags") or [])[:10],
        "escuchar_mp3": prev.get("preview-hq-mp3"),
        "pagina": s.get("url"),
    }


def _build_filter(min_duration, max_duration, license, extra_filter) -> str:
    parts = []
    if min_duration is not None or max_duration is not None:
        lo = "*" if min_duration is None else min_duration
        hi = "*" if max_duration is None else max_duration
        parts.append(f"duration:[{lo} TO {hi}]")
    lf = LICENSE_FILTERS.get(license, "")
    if lf:
        parts.append(lf)
    if extra_filter:
        parts.append(extra_filter)
    return " ".join(parts)


def _search(params: dict) -> dict:
    # /search/ is the current endpoint; /search/text/ kept as fallback for older servers
    try:
        return _get("/search/", params)
    except httpx.HTTPStatusError as e:
        if e.response.status_code in (404, 405):
            return _get("/search/text/", params)
        raise


def _safe(name: str) -> str:
    return re.sub(r"[^\w\-. ]+", "_", name).strip()[:80] or "sound"


def _oauth_load() -> dict | None:
    if TOKEN_FILE.exists():
        return json.loads(TOKEN_FILE.read_text())
    return None


def _oauth_save(data: dict) -> None:
    TOKEN_FILE.parent.mkdir(parents=True, exist_ok=True)
    data["obtained_at"] = time.time()
    TOKEN_FILE.write_text(json.dumps(data))
    os.chmod(TOKEN_FILE, 0o600)


def _oauth_token() -> str | None:
    tok = _oauth_load()
    if not tok:
        return None
    if time.time() - tok.get("obtained_at", 0) < tok.get("expires_in", 86400) - 60:
        return tok["access_token"]
    with _client() as c:
        r = c.post(f"{API}/oauth2/access_token/", data={
            "client_id": CLIENT_ID, "client_secret": API_KEY,
            "grant_type": "refresh_token", "refresh_token": tok["refresh_token"]})
        if r.status_code != 200:
            return None
        _oauth_save(r.json())
        return r.json()["access_token"]


def _append_credits(folder: Path, s: dict) -> None:
    line = (f'"{s.get("name")}" por {s.get("username")} — {s.get("url")} — '
            f'licencia: {s.get("license")} — bajado {date.today().isoformat()}\n')
    with open(folder / "CREDITOS.txt", "a", encoding="utf-8") as f:
        f.write(line)


# ---------- tools ----------

@mcp.tool()
def freesound_search(query: str,
                     min_duration: float | None = None,
                     max_duration: float | None = None,
                     license: str = "any",
                     sort: str = "score",
                     page_size: int = 15,
                     page: int = 1,
                     extra_filter: str = "") -> dict:
    """Busca sonidos en Freesound.
    license: "any" | "cc0" (dominio público) | "commercial_ok" (excluye no-comerciales).
    sort: score | duration_desc | duration_asc | created_desc | downloads_desc | rating_desc.
    extra_filter: sintaxis Solr de Freesound, ej. 'type:wav channels:2 samplerate:48000' o 'tag:field-recording'.
    Devuelve id, autor, duración, licencia, tags, link para escuchar (mp3) y página."""
    data = _search({
        "query": query,
        "filter": _build_filter(min_duration, max_duration, license, extra_filter),
        "sort": sort, "fields": FIELDS,
        "page_size": max(1, min(page_size, 150)), "page": page,
    })
    return {"total": data.get("count"), "resultados": [_summ(s) for s in data.get("results", [])]}


@mcp.tool()
def freesound_similar(sound_id: int, page_size: int = 15, license: str = "any") -> dict:
    """Busca sonidos que SUENAN parecido a otro (análisis de contenido, no palabras)."""
    flt = _build_filter(None, None, license, "")
    try:
        data = _get("/search/", {"similar_to": sound_id, "filter": flt, "fields": FIELDS, "page_size": page_size})
    except httpx.HTTPStatusError:
        data = _get(f"/sounds/{sound_id}/similar/", {"fields": FIELDS, "page_size": page_size})
    return {"resultados": [_summ(s) for s in data.get("results", []) if s.get("id") != sound_id]}


@mcp.tool()
def freesound_sound_info(sound_id: int) -> dict:
    """Ficha completa de un sonido (descripción, licencia, formato, tags)."""
    s = _get(f"/sounds/{sound_id}/", {"fields": FIELDS})
    out = _summ(s)
    out["descripcion"] = (s.get("description") or "")[:1500]
    out["licencia_url"] = s.get("license")
    return out


@mcp.tool()
def freesound_download(sound_ids: list[int], subfolder: str = "", quality: str = "original") -> dict:
    """Baja sonidos a la carpeta local (FREESOUND_DOWNLOAD_DIR/subfolder) y anota créditos en CREDITOS.txt.
    quality: "original" (archivo original, requiere freesound_login una vez) | "preview" (mp3 HQ, sin login).
    Si se pide original y no hay login, baja el preview y lo avisa."""
    folder = DOWNLOAD_DIR / _safe(subfolder) if subfolder else DOWNLOAD_DIR
    folder.mkdir(parents=True, exist_ok=True)
    token = _oauth_token() if quality == "original" else None
    done, notes = [], []
    with _client() as c:
        for sid in sound_ids:
            s = _get(f"/sounds/{sid}/", {"fields": FIELDS})
            base = f'{sid}_{_safe(s.get("name", "sound"))}'
            base = re.sub(r"\.(wav|aiff?|flac|mp3|ogg|m4a)$", "", base, flags=re.I)
            if token:
                r = c.get(f"{API}/sounds/{sid}/download/", headers={"Authorization": f"Bearer {token}"})
                ext = s.get("type") or "wav"
            else:
                if quality == "original":
                    notes.append(f"{sid}: sin login → bajé el preview mp3 (usá freesound_login para originales)")
                r = c.get(s["previews"]["preview-hq-mp3"])
                ext = "mp3"
            r.raise_for_status()
            path = folder / f"{base}.{ext}"
            path.write_bytes(r.content)
            _append_credits(folder, s)
            done.append({"id": sid, "archivo": str(path), "licencia": _short_license(s.get("license", ""))})
    return {"carpeta": str(folder), "bajados": done, "notas": notes}


@mcp.tool()
def freesound_login() -> dict:
    """Paso 1 para bajar originales: devuelve el link para autorizar la app con tu cuenta de Freesound.
    Después de autorizar, Freesound muestra un código: pasalo a freesound_login_complete."""
    if not CLIENT_ID:
        raise RuntimeError("Falta FREESOUND_CLIENT_ID en la config del MCP.")
    return {"abrir_en_el_navegador": f"{API}/oauth2/authorize/?client_id={CLIENT_ID}&response_type=code&state=mye"}


@mcp.tool()
def freesound_login_complete(code: str) -> dict:
    """Paso 2: canjea el código que mostró Freesound por un token (se guarda localmente y se renueva solo)."""
    with _client() as c:
        r = c.post(f"{API}/oauth2/access_token/", data={
            "client_id": CLIENT_ID, "client_secret": API_KEY,
            "grant_type": "authorization_code", "code": code.strip()})
        r.raise_for_status()
        _oauth_save(r.json())
    return {"ok": True, "mensaje": "Listo: ya podés bajar originales."}


if __name__ == "__main__":
    mcp.run()
