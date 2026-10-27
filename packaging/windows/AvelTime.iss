#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif

[Setup]
AppId={{6B0E8C7A-3D2F-4A51-9C1E-7D3B0A5E2C41}
AppName=Avel Time
AppVersion={#AppVersion}
AppPublisher=Avel
DefaultDirName={commonpf64}\Common Files\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir=..\..\dist
OutputBaseFilename=AvelTime-{#AppVersion}-Windows-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
UninstallDisplayName=Avel Time

[Files]
Source: "..\..\build\AvelTime_artefacts\Release\VST3\Avel Time.vst3\*"; DestDir: "{commonpf64}\Common Files\VST3\Avel Time.vst3"; Flags: recursesubdirs ignoreversion
