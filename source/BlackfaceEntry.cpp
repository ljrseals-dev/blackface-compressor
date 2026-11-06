#include "BlackfaceController.h"
#include "BlackfaceIds.h"
#include "BlackfaceProcessor.h"
#include "public.sdk/source/main/pluginfactory.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

bool InitModule() { return true; }
bool DeinitModule() { return true; }

BEGIN_FACTORY_DEF(BF_VENDOR, BF_URL, BF_EMAIL)

DEF_CLASS2(INLINE_UID_FROM_FUID(bf::kProcessorUID), PClassInfo::kManyInstances, kVstAudioEffectClass, BF_PLUGIN_NAME,
           Vst::kDistributable, Vst::PlugType::kFxDynamics, BF_VERSION_STR, kVstVersionString,
           bf::Processor::createInstance)

DEF_CLASS2(INLINE_UID_FROM_FUID(bf::kControllerUID), PClassInfo::kManyInstances, kVstComponentControllerClass,
           BF_PLUGIN_NAME " Controller", 0, "", BF_VERSION_STR, kVstVersionString, bf::Controller::createInstance)

END_FACTORY
