// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"
#include "MagicLeapSDKSettings.generated.h"

class ITargetPlatformManagerModule;
class IAndroidDeviceDetection;

/**
 * Implements the settings for the Magic Leap SDK setup.
 */
UCLASS(config = Engine, globaluserconfig, defaultconfig)
// Use defaultconfig here ^ so that MLSDKPath gets updated in the DefaultEngine.ini and matched with the config var used by UEBuildLumin.cs and MagicLeapPluginUtil.h
class LUMINPLATFORMEDITOR_API UMagicLeapSDKSettings : public UObject
{
public:
	GENERATED_BODY()
	
	UMagicLeapSDKSettings();
	
	/** Location on disk of the Magic Leap SDK (falls back to MLSDK environment variable if this is left blank) */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "SDK Config", Meta = (DisplayName = "Location of Magic Leap SDK", ConfigRestartRequired = true))
	FDirectoryPath MLSDKPath;

#if WITH_EDITOR
	/** UObject interface */
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	
	void SetTargetModule(ITargetPlatformManagerModule* TargetManagerModule);
	void SetDeviceDetection(IAndroidDeviceDetection* LuminDeviceDetection);
	void UpdateTargetModulePaths();
	
	ITargetPlatformManagerModule* TargetManagerModule;
	IAndroidDeviceDetection* LuminDeviceDetection;
#endif
};
