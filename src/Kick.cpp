#include "plugin.hpp"
#include "KickLayout.hpp"

struct Kick : Module {
	enum ParamId {
		PITCH_PARAM,
		SWEEP_PARAM,
		SWEEP_DECAY_PARAM,
		DECAY_PARAM,
		CLICK_PARAM,
		PARAMS_LEN
	};

	enum InputId {
		VOCT_INPUT,
		TRIG_INPUT,
		INPUTS_LEN
	};

	enum OutputId {
		OUT_OUTPUT,
		OUTPUTS_LEN
	};

	enum LightId {
		LIGHTS_LEN
	};

	float phase = 0.f;

	float kickAmplitude = 0.f;
	float kickPitch = 0.f;   // current pitch-sweep offset, in octaves

	// Click: noise phase-modulates the sine for the first few ms of each hit.
	// The CLICK knob sets the FM amount. More FM = louder click and a slightly brighter one.
	static constexpr float CLICK_MAX_INDEX = 3.5f;  // peak phase deviation at 100%, radians
	static constexpr float CLICK_HOLD = 0.001f;     // seconds at full strength
	static constexpr float CLICK_DECAY = 0.005f;    // seconds to -60 dB after the hold
	float clickEnv = 0.f;
	float clickHoldTimer = 0.f;

	// 24 dB/oct Butterworth lowpass = two 12 dB biquads in series with these Q values.
	// (A single biquad with Q 0.707 is the 12 dB Butterworth; the 4th-order one splits into 0.541 and 1.307.)
	static constexpr float LOWPASS_FREQ = 3700.f;
	dsp::BiquadFilter lowpass1, lowpass2;
	float filterSampleRate = 0.f;

	dsp::SchmittTrigger trigger;

	Kick() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Pitch is stored in octaves relative to C4 and displayed in Hz:
		// -3 oct = 32.7 Hz, -1 oct = 130.8 Hz, default about 53 Hz.
		configParam(PITCH_PARAM, -3.f, -1.f, -2.3f, "Pitch", " Hz", 2.f, dsp::FREQ_C4);
		configParam(SWEEP_PARAM, 0.f, 4.f, 2.5f, "Pitch sweep", " oct");
		configParam(SWEEP_DECAY_PARAM, 0.005f, 0.5f, 0.1f, "Pitch sweep decay", " ms", 0.f, 1000.f);
		configParam(DECAY_PARAM, 0.05f, 2.f, 0.275f, "Amplitude decay", " ms", 0.f, 1000.f);
		configParam(CLICK_PARAM, 0.f, 1.f, 0.6f, "Click", "%", 0.f, 100.f);
		configInput(VOCT_INPUT, "Pitch (1V/oct)");
		configInput(TRIG_INPUT, "Trigger");
		configOutput(OUT_OUTPUT, "Kick");	}

	void process(const ProcessArgs& args) override {
		float amplitudeDecay = params[DECAY_PARAM].getValue();
		float sweepDepth = params[SWEEP_PARAM].getValue();
		float sweepDecay = params[SWEEP_DECAY_PARAM].getValue();

		if (trigger.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 1.f)) {
			startKick(sweepDepth);
		}

		// Pitch in octaves (relative to C4): knob + 1V/oct input + sweep envelope.
		float pitch = params[PITCH_PARAM].getValue();
		pitch += inputs[VOCT_INPUT].getVoltage();
		pitch += kickPitch;

		float freq = dsp::FREQ_C4 * std::pow(2.f, pitch);
		// Keep the oscillator below Nyquist even with extreme CV.
		freq = clamp(freq, 0.f, args.sampleRate * 0.45f);

		updateKickAmplitude(args.sampleTime, amplitudeDecay);
		updateKickPitch(args.sampleTime, sweepDecay);

		phase += freq * args.sampleTime;
		if (phase >= 1.f)
			phase -= 1.f;

		updateClick(args.sampleTime);
		// White noise in -1..1, scaled by the click envelope, added to the sine's phase.
		float noise = 2.f * random::uniform() - 1.f;
		float clickIndex = CLICK_MAX_INDEX * params[CLICK_PARAM].getValue();
		float sine = std::sin(2.f * M_PI * phase + clickIndex * clickEnv * noise);

		// Filter coefficients depend on the sample rate, so recompute them only when it changes.
		if (args.sampleRate != filterSampleRate) {
			filterSampleRate = args.sampleRate;
			float f = LOWPASS_FREQ / args.sampleRate;
			lowpass1.setParameters(dsp::BiquadFilter::LOWPASS, f, 0.5412f, 1.f);
			lowpass2.setParameters(dsp::BiquadFilter::LOWPASS, f, 1.3066f, 1.f);
		}
		float out = lowpass2.process(lowpass1.process(sine * kickAmplitude));
		outputs[OUT_OUTPUT].setVoltage(5.f * out);	}

	void startKick(float sweepDepth) {
		// Restart the sine at phase 0 so every hit starts the same way (consistent transient).
		phase = 0.f;
		kickAmplitude = 1.f;
		kickPitch = sweepDepth;
		clickEnv = 1.f;
		clickHoldTimer = CLICK_HOLD;
	}

	// Instant attack, short hold at full strength, then the same exponential decay as the others.
	void updateClick(float deltaTime) {
		if (clickHoldTimer > 0.f) {
			clickHoldTimer -= deltaTime;
			return;
		}
		clickEnv *= std::exp(-6.9078f / CLICK_DECAY * deltaTime);
		if (clickEnv < 0.001f)
			clickEnv = 0.f;
	}

	// Both envelopes are exponential decays that reach about -60 dB after decayTime.
	// 6.9078 = ln(1000), and -60 dB is an amplitude of 1/1000.
	void updateKickAmplitude(float deltaTime, float decayTime) {
		float decayRate = 6.9078f / decayTime;
		kickAmplitude *= std::exp(-decayRate * deltaTime);
		if (kickAmplitude < 0.001f)
			kickAmplitude = 0.f;
	}

	void updateKickPitch(float deltaTime, float decayTime) {
		float decayRate = 6.9078f / decayTime;
		kickPitch *= std::exp(-decayRate * deltaTime);
		if (kickPitch < 0.001f)
			kickPitch = 0.f;
	}
};


struct KickWidget : ModuleWidget {
	KickWidget(Kick* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Kick.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Positions in mm, from KickLayout.hpp
		addParam(createParamCentered<Davies1900hLargeWhiteKnob>(mm2px(KickLayout::PITCH_PARAM), module, Kick::PITCH_PARAM));
		addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(KickLayout::SWEEP_PARAM), module, Kick::SWEEP_PARAM));
		addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(KickLayout::SWEEP_DECAY_PARAM), module, Kick::SWEEP_DECAY_PARAM));
		addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(KickLayout::DECAY_PARAM), module, Kick::DECAY_PARAM));
		addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(KickLayout::CLICK_PARAM), module, Kick::CLICK_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(KickLayout::VOCT_INPUT), module, Kick::VOCT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(KickLayout::TRIG_INPUT), module, Kick::TRIG_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(KickLayout::OUT_OUTPUT), module, Kick::OUT_OUTPUT));	}
};


Model* modelKick = createModel<Kick, KickWidget>("Kick");
