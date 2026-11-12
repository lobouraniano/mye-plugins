# MYE Freesound MCP

Buscá, escuchá y bajá sonidos de Freesound pidiéndoselo a Claude:
"traeme 10 puertas metálicas de más de 5 s, CC0, y bajalas a la carpeta *galpón*".

Herramientas: `freesound_search`, `freesound_similar` (sonidos que *suenan* parecido),
`freesound_sound_info`, `freesound_download` (anota créditos en `CREDITOS.txt`),
`freesound_login` / `freesound_login_complete` (una sola vez, para bajar originales).

## Instalación (Mac)

1. **Credenciales de Freesound**: entrá a https://freesound.org/apiv2/apply, creá una app.
   - Callback URL: `https://freesound.org/home/app_permissions/permission_granted/`
   - Anotá el **Client ID** y el **Client secret / Api key**.
2. **uv** (corre el script sin instalar nada más). En Terminal:
   ```
   curl -LsSf https://astral.sh/uv/install.sh | sh
   ```
3. **Bajá el script**: guardá `freesound_mcp.py` en `~/mye/freesound_mcp.py`.
4. **Config de Claude desktop**: Claude → Settings → Developer → Edit Config, y agregá:
   ```json
   {
     "mcpServers": {
       "freesound": {
         "command": "/Users/TU_USUARIO/.local/bin/uv",
         "args": ["run", "--script", "/Users/TU_USUARIO/mye/freesound_mcp.py"],
         "env": {
           "FREESOUND_API_KEY": "TU_API_KEY",
           "FREESOUND_CLIENT_ID": "TU_CLIENT_ID",
           "FREESOUND_DOWNLOAD_DIR": "/Users/TU_USUARIO/Music/Freesound"
         }
       }
     }
   }
   ```
5. Reiniciá Claude. La primera vez pedile "logueame en Freesound" para habilitar originales.

## Licencias
Cada descarga se anota en `CREDITOS.txt` con autor, link y licencia.
CC BY exige créditos; CC BY-NC **no** sirve para trabajos comerciales; CC0 es libre.
