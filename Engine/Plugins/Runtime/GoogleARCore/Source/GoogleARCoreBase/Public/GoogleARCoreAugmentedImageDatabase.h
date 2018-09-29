// Copyright 2017 Google Inc.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"

#include "GoogleARCoreAugmentedImageDatabase.generated.h"

#if PLATFORM_ANDROID
typedef struct ArAugmentedImageDatabase_ ArAugmentedImageDatabase;
#endif

USTRUCT(BlueprintType)
struct FGoogleARCoreAugmentedImageDatabaseEntry
{
	GENERATED_BODY()

	/**
	 * Name of the image. This can be retrieved from an active
	 * UGoogleARCoreAugmentedImage with the GetImageName function.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoogleARCore|AugmentedImages")
	FName Name;

	/**
	 * Texture to use for this image. Valid formats are RGBA8 and
	 * BGRA8.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoogleARCore|AugmentedImages")
	UTexture2D *ImageAsset;

	/**
	 * Width of the image in meters.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GoogleARCore|AugmentedImages")
	float Width;

	FGoogleARCoreAugmentedImageDatabaseEntry()
		: Width(0.0f) { }
};

UCLASS(BlueprintType)
class GOOGLEARCOREBASE_API UGoogleARCoreAugmentedImageDatabase : public UDataAsset
{
	GENERATED_BODY()

	friend class FGoogleARCoreSession;

public:
	/**
	 * Adds a single named image from the a UTexture2D to this AugmentedImageDatabase. This function will
	 * add a FGoogleARCoreAugmentedImageDatabaseEntry to the Entries property of this AugmentedImageDatabase.
	 *
	 * You need to restart the ARCore session with the config that contains this AugmentedImageDatabase to make
	 * track those new images.
	 * 
	 * You can set the ImageWidthInMeter value if the physical size of the image is known. This will
	 * help ARCore estimate the pose of the physical image as soon as ARCore detects the physical image.
	 * Otherwise, ARCore will requiring the user to move the device to view the physical image from
	 * different viewports.

	 * Note that this function takes time to perform non-trivial image processing (20ms - 30ms), and should
	 * be run on a background thread.
	 */
	UFUNCTION(BlueprintCallable, Category = "google arcore augmentedimages")
	int AddRuntimeAugmentedImageFromTexture(UTexture2D* ImageTexture, FName ImageName, float ImageWidthInMeter = 0);

	/**
	 * Adds a single named image from raw byte to this AugmentedImage database. This function will
	 * add a FGoogleARCoreAugmentedImageDatabaseEntry to the Entries property of this AugmentedImageDatabase.
	 * The ImageAsset property in FGoogleARCoreAugmentedImageDatabaseEntry will be null.
	 *
	 * You need to restart the ARCore session with the config that contains this AugmentedImageDatabase to make
	 * track those new images.
	 *
	 * You can set the ImageWidthInMeter value if the physical size of the image is known. This will
	 * help ARCore estimate the pose of the physical image as soon as ARCore detects the physical image.
	 * Otherwise, ARCore will requiring the user to move the device to view the physical image from
	 * different viewports.
	 *
	 * Note that this function takes time to perform non-trivial image processing (20ms - 30ms), and should
	 * be run on a background thread.
	 */
	int AddRuntimeAugmentedImage(const TArray<uint8>& ImageGrayscalePixels, int ImageWidth, int ImageHeight,
		FName ImageName, float ImageWidthInMeter = 0, UTexture2D* ImageTexture = nullptr);

	virtual void Serialize(FArchive& Ar) override;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GoogleARCore|AugmentedImages")
	TArray<FGoogleARCoreAugmentedImageDatabaseEntry> Entries;

	UPROPERTY()
	TArray<uint8> SerializedDatabase;

private:
#if PLATFORM_ANDROID
	ArAugmentedImageDatabase* NativeHandle = nullptr;
#endif
};

