# Instalar los plugins M.Y.E. en Mac

1. Copiá todo lo que hay en `VST3/` a `~/Library/Audio/Plug-Ins/VST3/`
2. Copiá todo lo que hay en `Components/` a `~/Library/Audio/Plug-Ins/Components/`
3. Abrí Terminal y corré (quita la cuarentena de macOS):

```
xattr -cr ~/Library/Audio/Plug-Ins/VST3/MYE* ~/Library/Audio/Plug-Ins/Components/MYE*
```

4. Si tenías **MYE Eco** u **MYE Oido** instalados, borralos: ahora se llaman **MYE Estribo** y **MYE Timpano**.
5. En Audition: Efectos → Administrador de efectos de audio → Escanear.

`VERSIONES.txt` dice qué versión de cada plugin trae este paquete.
