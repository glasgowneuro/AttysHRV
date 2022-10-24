JNIEnv *jni_env = Core::HAZEOS::GetJNIEnv();
jclass cls_Env = jni_env->FindClass("android/app/NativeActivity");
jmethodID mid_getExtStorage = jni_env->GetMethodID(cls_Env, "getFilesDir","()Ljava/io/File;");
jobject obj_File = jni_env->CallObjectMethod( gstate->activity->clazz, mid_getExtStorage);
jclass cls_File = jni_env->FindClass("java/io/File");
jmethodID mid_getPath = jni_env->GetMethodID(cls_File, "getPath","()Ljava/lang/String;");
jstring obj_Path = (jstring) jni_env->CallObjectMethod(obj_File, mid_getPath);
const char* path = jni_env->GetStringUTFChars(obj_Path, NULL);
FHZ_PRINTF("INTERNAL PATH = %s\n", path);
jni_env->ReleaseStringUTFChars(obj_Path, path);

// getCacheDir() - java
mid_getExtStorage = jni_env->GetMethodID( cls_Env,"getCacheDir", "()Ljava/io/File;");
obj_File = jni_env->CallObjectMethod(gstate->activity->clazz, mid_getExtStorage, NULL);
cls_File = jni_env->FindClass("java/io/File");
mid_getPath = jni_env->GetMethodID(cls_File, "getAbsolutePath", "()Ljava/lang/String;");
obj_Path = (jstring) jni_env->CallObjectMethod(obj_File, mid_getPath);
path = jni_env->GetStringUTFChars(obj_Path, NULL);
FHZ_PRINTF("CACHE DIR = %s\n", path); 
jni_env->ReleaseStringUTFChars(obj_Path, path);

// getExternalFilesDir() - java
mid_getExtStorage = jni_env->GetMethodID( cls_Env,"getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
obj_File = jni_env->CallObjectMethod(gstate->activity->clazz, mid_getExtStorage, NULL);
cls_File = jni_env->FindClass("java/io/File");
mid_getPath = jni_env->GetMethodID(cls_File, "getPath", "()Ljava/lang/String;");
obj_Path = (jstring) jni_env->CallObjectMethod(obj_File, mid_getPath);
path = jni_env->GetStringUTFChars(obj_Path, NULL);
FHZ_PRINTF("EXTERNAL PATH = %s\n", path);
jni_env->ReleaseStringUTFChars(obj_Path, path);




---------------------------------------------------------------------------------------

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
