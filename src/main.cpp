#include "fmod/fmod.hpp"
#include "fmod/fmod_studio.hpp"
#include "fmod/fmod_errors.h"

#include "phonon/phonon.h"
#include "phonon_fmod/steamaudio_fmod.h"
#include "phonon_fmod/dsp_names.h"

#include <cstdio>

//
//	Somewhere in this code the reflection simultion is misconfigured in a way that results in an access violation read on 0xFFFFF...
//	when the Steam Audio Spatializer / Steam Reverb plugins call iplReflectionEffectApply.
// 
//  I removed the reverb effect to make this more simple
//
//

// Enable this to cause the error (switches on reflections in fmod plugin)
//
bool gCauseError = true;

// Stop the main loop if fmod has run into an error
//
bool gHasError = false;

void fa(int result)
{
	if (result != FMOD_OK)
	{
		printf("FMOD error %d: %s\n", result, FMOD_ErrorString((FMOD_RESULT)result));
		gHasError = true;
	}
}

IPLSimulationFlags IPL_DIRECT_AND_REFLECT = (IPLSimulationFlags)(
	  IPL_SIMULATIONFLAGS_DIRECT
	| IPL_SIMULATIONFLAGS_REFLECTIONS
);

IPLDirectSimulationFlags IPL_DIRECT_AND_REFLECT_DIRECT_SIM = (IPLDirectSimulationFlags)(
	  IPL_DIRECTSIMULATIONFLAGS_OCCLUSION
	| IPL_DIRECTSIMULATIONFLAGS_TRANSMISSION
);

int main()
{
	// Create FMOD Studio system

	FMOD::Studio::System* fmodSystem;
	fa(FMOD::Studio::System::create(&fmodSystem));

	fa(fmodSystem->initialize(16, FMOD_STUDIO_INIT_NORMAL, FMOD_INIT_NORMAL, nullptr));

	// Load phonon plugin

	FMOD::System* fmodCore;
	fa(fmodSystem->getCoreSystem(&fmodCore));
	fa(fmodCore->loadPlugin("phonon_fmod.dll", nullptr));

	// Load FMOD banks

	FMOD::Studio::Bank* fmodBank;
	FMOD::Studio::Bank* fmodBankStrings;

	fa(fmodSystem->loadBankFile("../assets/Master.bank", FMOD_STUDIO_LOAD_BANK_NORMAL, &fmodBank));
	fa(fmodSystem->loadBankFile("../assets/Master.strings.bank", FMOD_STUDIO_LOAD_BANK_NORMAL, &fmodBankStrings));

	// Load the example event and play it

	FMOD::Studio::EventDescription* desc;
	fa(fmodSystem->getEvent("event:/example", &desc));

	FMOD::Studio::EventInstance* fmodExampleAudioInstance;
	fa(desc->createInstance(&fmodExampleAudioInstance));

	fa(fmodExampleAudioInstance->start());

	// Set the event's position

	FMOD_3D_ATTRIBUTES exampleAudioPosition = {};
	exampleAudioPosition.up.y = 1;
	exampleAudioPosition.forward.z = 1;
	exampleAudioPosition.position.x = 2;

	fmodExampleAudioInstance->set3DAttributes(&exampleAudioPosition);

	//
	// Configure phonon
	//

	// Create context

	IPLContext iplSystem;

	IPLContextSettings vsettings = {};
	vsettings.version = STEAMAUDIO_VERSION;

	iplContextCreate(&vsettings, &iplSystem);

	// Create hrtf
	
	IPLAudioSettings audioSettings = {};
	audioSettings.samplingRate = 44100;
	audioSettings.frameSize = 1024;

	IPLHRTFSettings hrtfSettings = {};
	hrtfSettings.type = IPL_HRTFTYPE_DEFAULT;

	IPLHRTF hrtf = nullptr;
	iplHRTFCreate(iplSystem, &audioSettings, &hrtfSettings, &hrtf);

	// Create a simulator for direct and reflection simulations

	IPLSimulator iplSimulator;

	IPLSimulationSettings simulationSettings = {};
	simulationSettings.flags = IPL_DIRECT_AND_REFLECT;
	simulationSettings.sceneType = IPL_SCENETYPE_DEFAULT;
	simulationSettings.reflectionType = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION;
	simulationSettings.maxNumRays = 4096;
	simulationSettings.numDiffuseSamples = 32;
	simulationSettings.maxDuration = 2.0f;
	simulationSettings.maxNumSources = 8;
	simulationSettings.numThreads = 2;
	simulationSettings.samplingRate = audioSettings.samplingRate;
	simulationSettings.frameSize = audioSettings.frameSize;

	// here is maxOrder
	// https://github.com/ValveSoftware/steam-audio/issues/243#issuecomment-1444127503
	simulationSettings.maxOrder = 1;

	iplSimulatorCreate(iplSystem, &simulationSettings, &iplSimulator);

	// Create scene

	IPLScene iplScene;

	IPLSceneSettings sceneSettings = {};
	sceneSettings.type = simulationSettings.sceneType;

	iplSceneCreate(iplSystem, &sceneSettings, &iplScene);
	iplSimulatorSetScene(iplSimulator, iplScene);

	// Tell FMOD plugin about the state of phonon

	iplFMODInitialize(iplSystem);
	iplFMODSetHRTF(hrtf);
	iplFMODSetSimulationSettings(simulationSettings);

	// Add the FMOD audio event to the simulator

	IPLSourceSettings sourceSettings = {};
	sourceSettings.flags = IPL_DIRECT_AND_REFLECT;

	IPLSource iplExampleSource;
	iplSourceCreate(iplSimulator, &sourceSettings, &iplExampleSource);
	iplSourceAdd(iplExampleSource, iplSimulator);

	// Apply changes to the scene / simulator

	iplSceneCommit(iplScene);
	iplSimulatorCommit(iplSimulator);

	// wait until the example event has started so DSPs are created

	FMOD_STUDIO_PLAYBACK_STATE state;
	do
	{
		fa(fmodSystem->update());
		fa(fmodExampleAudioInstance->getPlaybackState(&state));
	}
	while (state != FMOD_STUDIO_PLAYBACK_PLAYING && !gHasError);

	//
	//	Enter sim loop
	//

	while (!gHasError)
	{
		// The listener position is (0, 0, 0)

		IPLCoordinateSpace3 listenerPos = {};
		listenerPos.right.x = 1;
		listenerPos.up.y = 1;
		listenerPos.ahead.z = 1;

		// Set simulation settings

		IPLSimulationSharedInputs sharedSimInputs = {};
		sharedSimInputs.listener = listenerPos;
		sharedSimInputs.duration = 1.f;
		sharedSimInputs.numRays = 3000;
		sharedSimInputs.numBounces = 5;
		sharedSimInputs.irradianceMinDistance = .1f;
		sharedSimInputs.order = 1;

		iplSimulatorSetSharedInputs(iplSimulator, IPL_DIRECT_AND_REFLECT, &sharedSimInputs);

		// Set direct audio settings for the iplExampleSource

		IPLCoordinateSpace3 sourcePos = listenerPos;
		sourcePos.origin.x = exampleAudioPosition.position.x;
		sourcePos.origin.y = exampleAudioPosition.position.y;
		sourcePos.origin.z = exampleAudioPosition.position.z;
		
		IPLSimulationInputs exampleSourceInputs = {};
		exampleSourceInputs.flags = IPL_DIRECT_AND_REFLECT;
		exampleSourceInputs.directFlags = IPL_DIRECT_AND_REFLECT_DIRECT_SIM;
		exampleSourceInputs.occlusionType = IPL_OCCLUSIONTYPE_RAYCAST;
		exampleSourceInputs.source = sourcePos;

		iplSourceSetInputs(iplExampleSource, IPL_DIRECT_AND_REFLECT, &exampleSourceInputs);

		// Run simulations

		iplSimulatorRunDirect(iplSimulator);
		iplSimulatorRunReflections(iplSimulator);

		// Set the dsp parameters for the phonon fmod plugins

		FMOD::ChannelGroup* group;
		FMOD::DSP* dsp;

		fa(fmodExampleAudioInstance->getChannelGroup(&group));
		fa(group->getDSP(0, &dsp)); // assumes 0 is the Steam Audio Spatializer

		dsp->setParameterInt(SpatializeEffect::APPLY_OCCLUSION, 1);

		// Setting this to true causes the error as it applies a reflection effect

		dsp->setParameterBool(SpatializeEffect::APPLY_REFLECTIONS, gCauseError);

		// Docs say to set this to a pointer to the output,
		// but it seems like the plugins actually expect the source and read the
		// outputs themselves

		dsp->setParameterData(SpatializeEffect::SIMULATION_OUTPUTS, (void**)&iplExampleSource, sizeof(IPLSource*));

		// Tick FMOD

		fmodSystem->update();
	}
}