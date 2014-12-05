#ifndef DistortionCS2x2_refl

const OVR::CAPI::D3D_NS::ShaderBase::Uniform DistortionCS2x2_refl[] =
{
	{ "Padding1", 	OVR::CAPI::D3D_NS::ShaderBase::VARTYPE_FLOAT, 0, 64 },
	{ "Padding2", 	OVR::CAPI::D3D_NS::ShaderBase::VARTYPE_FLOAT, 64, 64 },
	{ "EyeToSourceUVScale", 	OVR::CAPI::D3D_NS::ShaderBase::VARTYPE_FLOAT, 128, 8 },
	{ "EyeToSourceUVOffset", 	OVR::CAPI::D3D_NS::ShaderBase::VARTYPE_FLOAT, 136, 8 },
	{ "EyeRotationStart", 	OVR::CAPI::D3D_NS::ShaderBase::VARTYPE_FLOAT, 144, 44 },
	{ "EyeRotationEnd", 	OVR::CAPI::D3D_NS::ShaderBase::VARTYPE_FLOAT, 192, 44 },
	{ "UseOverlay", 	OVR::CAPI::D3D_NS::ShaderBase::VARTYPE_FLOAT, 236, 4 },
	{ "RightEye", 	OVR::CAPI::D3D_NS::ShaderBase::VARTYPE_FLOAT, 240, 4 },
	{ "FbSizePixelsX", 	OVR::CAPI::D3D_NS::ShaderBase::VARTYPE_FLOAT, 244, 4 },
};

#endif
