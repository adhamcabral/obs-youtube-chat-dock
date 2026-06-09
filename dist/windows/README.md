# OBS YouTube Chat Dock for Windows

OBS YouTube Chat Dock is a simple OBS Studio plugin that adds a dock for
YouTube live chat.

Paste a YouTube channel link once. The plugin checks the channel `/live` page
periodically and automatically loads the YouTube live chat pop-out inside OBS.

## Requirements

- Windows 64-bit
- OBS Studio 32.x
- `obs-browser` plugin available in OBS

## Install

1. Close OBS before installing or updating the plugin.
2. Extract this zip file.
3. Run:

```bat
windowsplugin.bat
```

The installer copies `chat-dock.dll` to:

```txt
C:\ProgramData\obs-studio\plugins\chat-dock\bin\64bit
```

Depending on your Windows permissions, you may need to run the installer as
administrator.

## Usage

1. Open OBS.
2. Open:

```txt
Docks > Chat
```

3. Paste a YouTube channel or live URL, for example:

```txt
https://www.youtube.com/@channel
https://www.youtube.com/channel/UC...
https://www.youtube.com/watch?v=VIDEO_ID
```

4. Click `Salvar` or `Checar`.

When a live stream is detected, the dock opens the YouTube pop-out chat:

```txt
https://www.youtube.com/live_chat?v=VIDEO_ID&is_popout=1
```

## Notes

This Windows binary was built for OBS 32.x. It should work while the OBS and
`obs-browser` ABI remains compatible.

The plugin uses the public YouTube page to detect live streams. This avoids an
API key, but it can break if YouTube changes the page HTML.
