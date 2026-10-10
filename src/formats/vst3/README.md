# VST3 Adapter

This directory is intentionally framework/SDK-facing only.

The VST3 adapter should:
- translate VST3 audio/event/process structures into StudioHub engine calls
- expose parameters and automation
- implement state save/restore
- report latency
- handle sample-rate/block-size changes
- keep VST3 SDK types out of DSP headers

Enable only after the SDK is supplied/configured.
