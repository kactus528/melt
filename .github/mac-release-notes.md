Universal (Apple Silicon + Intel) build of **Melt**: VST3, Audio Unit and a standalone app, macOS 10.13 or newer.

**Install**
1. Download `Melt-Mac.zip` below and double-click it to unzip.
2. Copy `Melt.vst3` to `~/Library/Audio/Plug-Ins/VST3/` and `Melt.component` to `~/Library/Audio/Plug-Ins/Components/`
   (Finder: Go > Go to Folder... and paste the path).
3. It isn't notarized by Apple, so clear the download flag in Terminal:
   `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Melt.vst3 ~/Library/Audio/Plug-Ins/Components/Melt.component`
4. In Ableton Live: Settings > Plug-Ins > turn on "Use VST3 Plug-in System Folders" (and "Use Audio Units"), then Rescan.

Projects and presets are interchangeable with the Windows version.
