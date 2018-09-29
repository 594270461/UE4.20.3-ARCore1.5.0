#
# Build LowLevelCloth
#
SET(PHYSX_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../)

SET(LL_SOURCE_DIR ${PHYSX_SOURCE_DIR}/LowLevelCloth/src)

SET(LOWLEVELCLOTH_PLATFORM_INCLUDES
	${PHYSX_SOURCE_DIR}/LowLevelAABB/mac/include
)

SET(LOWLEVELCLOTH_COMPILE_DEFS

	# Common to all configurations
	${PHYSX_MAC_COMPILE_DEFS};PX_PHYSX_STATIC_LIB

	$<$<CONFIG:debug>:${PHYSX_MAC_DEBUG_COMPILE_DEFS};PX_PHYSX_DLL_NAME_POSTFIX=DEBUG;>
	$<$<CONFIG:checked>:${PHYSX_MAC_CHECKED_COMPILE_DEFS};PX_PHYSX_DLL_NAME_POSTFIX=CHECKED;>
	$<$<CONFIG:profile>:${PHYSX_MAC_PROFILE_COMPILE_DEFS};PX_PHYSX_DLL_NAME_POSTFIX=PROFILE;>
	$<$<CONFIG:release>:${PHYSX_MAC_RELEASE_COMPILE_DEFS};>
)

# include common low level cloth settings
INCLUDE(../common/LowLevelCloth.cmake)


# enable -fPIC so we can link static libs with the editor
SET_TARGET_PROPERTIES(LowLevelCloth PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
