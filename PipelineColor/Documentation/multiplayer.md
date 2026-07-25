# Multiplayer

## Client requirement

PipelineColor sets **`RequiredOnRemote: true`**. Every player joining a session must have the same mod version installed.

Version range in the plugin: `^1.3.0` (compatible 1.3.x).

## Listen server / host

The host (or dedicated server) owns apply, store, and config mutation. Clients receive replicated customization data; they do not run authority apply or write cfg.

Chat bangs and `PipelineColor.Set` apply on the **authority** only. Pure clients get `Host only.` Any connected player may edit Customizer PC swatches and run bangs on a listen/dedicated session (authority executes; cfg is shared on the host machine).

## Dedicated server

Release builds should include server targets (Windows Server / Linux Server) before an SMR upload. Prefer Alpakit Release → combined zip.

### Server config path

```text
<Satisfactory>/Configs/PipelineColor.cfg
```

Edit via console, chat when connected as authority, or edit the cfg file on the host. See [chat-commands.md](chat-commands.md).

Metallic defaults / overrides and **color-source** overrides live in cfg (not the save). `CfgSchema` 2 uses prefixed catalog keys.

## Save compatibility

Customizer / SaveGame swatch entries persist in the world save (**schema 4** — only custom edits). Runtime pipe/support paint rebuilds from fluid state + store overlay on load. Schema &lt; 4 nukes once on load.

## Troubleshooting MP

| Symptom | Check |
|---------|--------|
| Mod missing on join | All parties on same 1.3.x, SML ^3.12.0 |
| Mod fluid sections differ host vs client | Same optional fluid mods (SFP/RP/…) on every machine; sections follow discovered descriptors on authority |
| Colors only on host | Dedicated build includes the PipelineColor server target |
| Chat ignored | Run on server/listen host; pure client gets `Host only.` |
| Customizer edits lost | Save after editing; pre-1.3.0 store nuke; store is SaveGame-backed |

## Related

- [Troubleshooting](troubleshooting.md)
- [Player guide](player-guide.md)
