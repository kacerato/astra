// Spike S-02: os dois símbolos que o renderer Vulkan do The Forge pede da camada de janela do TF
// (OS/Android/AndroidWindow.cpp e AndroidBase.cpp), fornecidos aqui sem o glue do NativeActivity.
//
// Vulkan.c usa gWindow.handle.activity->{vm,clazz} para inicializar o Swappy (frame pacing). A plataforma
// Astra preenche gWindow com uma ANativeActivity "de fachada" montada a partir do GameActivity
// (mesmos campos: vm, clazz = objeto Java da Activity, caminhos, AAssetManager).

#include <jni.h>

#include "Common_3/OS/Interfaces/IOperatingSystem.h"

WindowDesc gWindow = {};

extern "C" jint AndroidAttachToCurrentThread(WindowDesc* pWindow, JNIEnv** ppEnv)
{
    return pWindow->handle.activity->vm->AttachCurrentThread(ppEnv, nullptr);
}
