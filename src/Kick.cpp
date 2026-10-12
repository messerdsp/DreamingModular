#include "plugin.hpp"
#include "KickLayout.hpp"

struct Kick : Module {
	enum ParamId {
		PITCH_PARAM,
		SWEEP_PARAM,
		SWEEP_DECAY_PARAM,
		DECAY_PARAM,
		PITCH_CV_PARAM,     // attenuverter for PITCH_CV_INPUT
		PARAMS_LEN
	};

	enum InputId {
		PITCH_CV_INPUT,
		TRIG_INPUT,
		INPUTS_LEN
	};

	enum OutputId {
		OUT_OUTPUT,
		OUTPUTS_LEN
	};

	enum LightId {
		TRIG_LIGHT,
		LIGHTS_LEN
	};

	float phase = 0.f;

	float kickAmplitude = 0.f;
	float kickPitch = 0.f;   // current pitch-sweep offset, in octaves

	dsp::SchmittTrigger trigger;

	Kick() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Pitch is stored in octaves relative to C4 and displayed in Hz:
		// -3 oct = 32.7 Hz, -1 oct = 130.8 Hz, default about 53 Hz.
		configParam(PITCH_PARAM, -3.f, -1.f, -2.3f, "Pitch", " Hz", 2.f, dsp::FREQ_C4);
		configParam(SWEEP_PARAM, 0.f, 4.f, 2.5f, "Pitch sweep", " oct");
		configParam(SWEEP_DECAY_PARAM, 0.005f, 0.5f, 0.1f, "Pitch sweep decay", " ms", 0.f, 1000.f);
		configParam(DECAY_PARAM, 0.05f, 2.f, 0.5f, "Amplitude decay", " ms", 0.f, 1000.f);
		// Attenuverter: -100%..+100%. Default +100% so the CV input tracks 1V/oct.
		configParam(PITCH_CV_PARAM, -1.f, 1.f, 1.f, "Pitch CV amount", "%", 0.f, 100.f);

		configInput(PITCH_CV_INPUT, "Pitch CV (1V/oct at 100%)");
		configInput(TRIG_INPUT, "Trigger");
		configOutput(OUT_OUTPUT, "Kick");
		configLight(TRIG_LIGHT, "Trigger");
	}

	void process(const ProcessArgs& args) override {
		float amplitudeDecay = params[DECAY_PARAM].getValue();
		float sweepDepth = params[SWEEP_PARAM].getValue();
		float sweepDecay = params[SWEEP_DECAY_PARAM].getValue();

		if (trigger.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 1.f)) {
			startKick(sweepDepth);
		}

		// Pitch in octaves (relative to C4): knob + attenuated CV + sweep envelope.
		float pitch = params[PITCH_PARAM].getValue();
		pitch += params[PITCH_CV_PARAM].getValue() * inputs[PITCH_CV_INPUT].getVoltage();
		pitch += kickPitch;

		float freq = dsp::FREQ_C4 * std::pow(2.f, pitch);
		// Keep the oscillator below Nyquist even with extreme CV.
		freq = clamp(freq, 0.f, args.sampleRate * 0.45f);

		updateKickAmplitude(args.sampleTime, amplitudeDecay);
		updateKickPitch(args.sampleTime, sweepDecay);

		phase += freq * args.sampleTime;
		if (phase >= 1.f)
			phase -= 1.f;

		float sine = std::sin(2.f * M_PI * phase);
		outputs[OUT_OUTPUT].setVoltage(5.f * sine * kickAmplitude);

		lights[TRIG_LIGHT].setBrightnessSmooth(kickAmplitude, args.sampleTime);
	}

	void startKick(float sweepDepth) {
		// Restart the sine at phase 0 so every hit starts the same way (consistent transient).
		phase = 0.f;
		kickAmplitude = 1.f;
		kickPitch = sweepDepth;
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
		addParam(createParamCentered<Trimpot>(mm2px(KickLayout::PITCH_CV_PARAM), module, Kick::PITCH_CV_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(KickLayout::PITCH_CV_INPUT), module, Kick::PITCH_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(KickLayout::TRIG_INPUT), module, Kick::TRIG_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(KickLayout::OUT_OUTPUT), module, Kick::OUT_OUTPUT));

		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(KickLayout::TRIG_LIGHT), module, Kick::TRIG_LIGHT));
	}
};


Model* modelKick = createModel<Kick, KickWidget>("Kick");
