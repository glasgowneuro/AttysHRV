#include <android/asset_manager.h>


AssetEnumerateFileType(app_->activity->assetManager, ".png", files);

AAssetManager* mgr = AAssetManager_fromJava(env, assetManager);
    AAsset* asset = AAssetManager_open(mgr, (const char *) js, AASSET_MODE_UNKNOWN);
    if (NULL == asset) {
        __android_log_print(ANDROID_LOG_ERROR, NF_LOG_TAG, "_ASSET_NOT_FOUND_");
        return JNI_FALSE;
    }
    long size = AAsset_getLength(asset);
    char* buffer = (char*) malloc (sizeof(char)*size);
    AAsset_read (asset,buffer,size);
    __android_log_print(ANDROID_LOG_ERROR, NF_LOG_TAG, buffer);
    AAsset_close(asset);
