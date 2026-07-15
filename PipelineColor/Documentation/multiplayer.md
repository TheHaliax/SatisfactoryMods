# Multiplayer

## Client requirement

PipelineColor sets **`RequiredOnRemote: true`**. Every player joining a session must have the same mod version installed.

Version range in the plugin: `^1.0.0` (compatible 1.0.x).

## Listen server / host

The host (or dedicated server) owns apply, store, and config mutation. Clients receive replicated customization data; they do not run authority apply or write cfg.

Chat bangs and `PipelineColor.Set` apply on the **authority** only.

## Dedicated server

Release builds should include server targets (Windows Server / Linux Server) before an SMR upload. Prefer Alpakit Release → combined zip.

### Server config path

```text
<Satisfactory>/Configs/PipelineColor.cfg
```

Edit via console, chat when connected as authority, or edit the cfg file on the host. See [chat-commands.md](chat-commands.md).

## Save compatibility

Customizer / SaveGame swatch entries persist in the world save. Runtime pipe/support paint rebuilds from fluid state + store on load. Metallic defaults and overrides live in cfg (not the save).

## Troubleshooting MP

| Symptom | Check |
|---------|--------|
| Mod missing on join | All parties on same 1.0.x, SML ^3.12.0 |
| Colors SP only | Server target includes PipelineColor DLL |
| Chat ignored | Run on server/listen host; pure client gets Hal error |
| Customizer edits lost | Save the game after editing; store is SaveGame-backed |

## Related

- [Troubleshooting](troubleshooting.md)
- [Player guide](player-guide.md)
