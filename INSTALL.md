# Instalar MYE Eco en Mac

1. Copiá `VST3/MYE Eco.vst3` a `~/Library/Audio/Plug-Ins/VST3/`
2. Copiá `Components/MYE Eco.component` a `~/Library/Audio/Plug-Ins/Components/`
3. Abrí Terminal y corré (quita la cuarentena de macOS):

```
xattr -cr ~/Library/Audio/Plug-Ins/VST3/"MYE Eco.vst3" ~/Library/Audio/Plug-Ins/Components/"MYE Eco.component"
```

4. En Audition: Efectos → Administrador de efectos de audio → Escanear.
