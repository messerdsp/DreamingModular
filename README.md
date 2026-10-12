# Dreaming Modular

Drum modules for [VCV Rack 2](https://vcvrack.com/).

**Early WIP, v1 sound in progress.**

## Kick

A synthesized kick drum built to cover a wide range of styles: realistic acoustic kicks, 808 and digital kicks, and more experimental sounds.

A recorded kick is really two sounds: the near-field mic inside the drum (short and punchy, with the click and the sub) and the far-field room mics (longer and boomier, with less sub). The Kick synthesizes each with its own FM voice and mixes them together. The approach is inspired by [Zion Jaymes](https://www.youtube.com/@zionjaymes)' kick synthesis technique.

Working now:

- Sine with an exponential pitch sweep (depth and decay)
- Exponential amplitude decay
- Trigger input, pitch CV input with attenuverter

Next: noise-FM click, far-field path, near/far mix, stereo outputs.

## Building

Requires the [VCV Rack SDK](https://vcvrack.com/manual/PluginDevelopmentTutorial).

```sh
make install
```

Set `RACK_DIR` if the SDK isn't at `../SDK/Rack-SDK`.

## License

[GPL-3.0-or-later](LICENSE) © Reid Messer
