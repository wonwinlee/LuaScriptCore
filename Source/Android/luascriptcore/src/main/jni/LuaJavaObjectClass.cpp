//
// Created by 冯鸿杰 on 16/10/8.
//

#include <stdint.h>
#include <jni.h>
#include <sys/time.h>
#include <stdio.h>
#include "LuaJavaObjectClass.h"
#include "LuaJavaType.h"
#include "LuaJavaEnv.h"
#include "LuaDefine.h"
#include "LuaObjectManager.h"
#include "LuaJavaConverter.h"
#include "LuaJavaObjectDescriptor.h"
#include "LuaPointer.h"

/**
 * 类实例化处理器
 *
 * @param instance 实例对象
 */
static void _luaClassObjectCreated (cn::vimfung::luascriptcore::modules::oo::LuaObjectClass *objectClass)
{
    using namespace cn::vimfung::luascriptcore;

    JNIEnv *env = LuaJavaEnv::getEnv();

    LuaJavaObjectClass *jobjectClass = (LuaJavaObjectClass *)objectClass;
    jclass cls = jobjectClass -> getModuleClass(env);
    if (cls != NULL)
    {
        lua_State *state = jobjectClass->getContext()->getLuaState();

        //创建实例对象
        jmethodID initMethodId = env->GetMethodID(cls, "<init>", "()V");

        //创建Java层的实例对象
        jobject jcontext = LuaJavaEnv::getJavaLuaContext(env, objectClass->getContext());
        jobject jInstance = env->NewObject(cls, initMethodId);

        LuaJavaObjectDescriptor *objDesc = new LuaJavaObjectDescriptor(env, jInstance);
        //创建Lua中的实例对象
        objectClass -> createLuaInstance(objDesc);

        //调用实例对象的init方法
        lua_pushvalue(state, 1);
        lua_getfield(state, -1, "init");
        if (lua_isfunction(state, -1))
        {
            lua_pushvalue(state, -2);
            lua_pcall(state, 1, 0, 0);
            lua_pop(state, 1);
        }
        else
        {
            lua_pop(state, 2);
        }

        objDesc -> release();
        env -> DeleteLocalRef(jInstance);
    }

    LuaJavaEnv::resetEnv(env);
}

static void _luaClassObjectDestroy (cn::vimfung::luascriptcore::modules::oo::LuaObjectClass *objectClass)
{
    using namespace cn::vimfung::luascriptcore;

    lua_State *state = objectClass -> getContext() -> getLuaState();

    if (lua_gettop(state) > 0 && lua_isuserdata(state, 1))
    {
        JNIEnv *env = LuaJavaEnv::getEnv();

        //表示有实例对象传入
        LuaUserdataRef ref = (LuaUserdataRef)lua_touserdata(state, 1);
        LuaObjectDescriptor *objDesc = (LuaObjectDescriptor *)ref -> value;
        jobject instance = (jobject)objDesc -> getObject();

        LuaJavaEnv::removeAssociateInstance(env, instance);

        //调用实例对象的destroy方法
        lua_pushvalue(state, 1);
        lua_getfield(state, -1, "destroy");
        if (lua_isfunction(state, -1))
        {
            lua_pushvalue(state, 1);
            lua_pcall(state, 1, 0, 0);
        }
        lua_pop(state, 2);

        //移除对象引用
        objDesc -> release();

        LuaJavaEnv::resetEnv(env);
    }
}

static std::string _luaClassObjectDescription (cn::vimfung::luascriptcore::modules::oo::LuaObjectClass *objectClass)
{
    using namespace cn::vimfung::luascriptcore;

    std::string str;

    JNIEnv *env = LuaJavaEnv::getEnv();

    lua_State *state = objectClass -> getContext() -> getLuaState();

    if (lua_gettop(state) > 0 && lua_isuserdata(state, 1))
    {
        //表示有实例对象传入
        LuaUserdataRef ref = (LuaUserdataRef)lua_touserdata(state, 1);
        jobject instance = (jobject)((LuaObjectDescriptor *)ref -> value) -> getObject();

        jclass cls = env -> GetObjectClass(instance);
        jmethodID toStringMethodId = env -> GetMethodID(cls, "toString", "()Ljava/lang/String;");

        jstring desc = (jstring)env -> CallObjectMethod(instance, toStringMethodId);
        const char *descCStr = env -> GetStringUTFChars(desc, NULL);
        str = descCStr;
        env -> ReleaseStringUTFChars(desc, descCStr);
    }

    LuaJavaEnv::resetEnv(env);

    return str;
}

/**
 * Lua类方法处理器
 *
 * @param module 类模块
 * @param methodName 方法名称
 * @param arguments 参数列表
 *
 * @return 返回值
 */
static LuaValue* _luaClassMethodHandler(LuaModule *module, std::string methodName, LuaArgumentList arguments)
{
    JNIEnv *env = LuaJavaEnv::getEnv();
    LuaValue *retValue = NULL;

    LuaJavaObjectClass *jmodule = (LuaJavaObjectClass *)module;
    if (jmodule != NULL)
    {
        static jclass moduleClass = jmodule -> getModuleClass(env);
        static jmethodID invokeMethodID = env -> GetStaticMethodID(LuaJavaType::moduleClass(env), "_methodInvoke", "(Ljava/lang/Class;Ljava/lang/String;[Lcn/vimfung/luascriptcore/LuaValue;)Lcn/vimfung/luascriptcore/LuaValue;");

        static jclass luaValueClass = LuaJavaType::luaValueClass(env);

        jstring jMethodName = env -> NewStringUTF(methodName.c_str());

        //参数
        jobjectArray argumentArr = env -> NewObjectArray(arguments.size(), luaValueClass, NULL);
        int index = 0;
        for (LuaArgumentList::iterator it = arguments.begin(); it != arguments.end(); it ++)
        {
            LuaValue *argument = *it;
            jobject jArgument = LuaJavaConverter::convertToJavaLuaValueByLuaValue(env, jmodule -> getContext(), argument);
            env -> SetObjectArrayElement(argumentArr, index, jArgument);
            index++;
        }

        jobject result = env -> CallStaticObjectMethod(moduleClass, invokeMethodID, moduleClass, jMethodName, argumentArr);
        retValue = LuaJavaConverter::convertToLuaValueByJLuaValue(env, result);
    }

    LuaJavaEnv::resetEnv(env);

    if (retValue == NULL)
    {
        retValue = new LuaValue();
    }

    return retValue;
}


/**
 * Lua类实例方法处理器
 *
 * @param module 模块对象
 * @param methodName 方法名称
 * @param arguments 方法参数列表
 *
 * @return 方法返回值
 */
static LuaValue* _luaInstanceMethodHandler (cn::vimfung::luascriptcore::modules::oo::LuaObjectClass *objectClass, std::string methodName, LuaArgumentList arguments)
{
    using namespace cn::vimfung::luascriptcore;
    LuaValue *retValue = NULL;

    JNIEnv *env = LuaJavaEnv::getEnv();

    lua_State *state = objectClass -> getContext() -> getLuaState();

    if (lua_gettop(state) > 0 && lua_isuserdata(state, 1))
    {
        //表示有实例对象传入
        LuaUserdataRef ref = (LuaUserdataRef)lua_touserdata(state, 1);
        jobject instance = (jobject)((LuaObjectDescriptor *)ref -> value) -> getObject();

        jmethodID invokeMethodID = env -> GetMethodID(LuaJavaType::luaObjectClass(env), "_instanceMethodInvoke", "(Ljava/lang/String;[Lcn/vimfung/luascriptcore/LuaValue;)Lcn/vimfung/luascriptcore/LuaValue;");

        static jclass luaValueClass = LuaJavaType::luaValueClass(env);

        jstring jMethodName = env -> NewStringUTF(methodName.c_str());

        //参数
        jobjectArray argumentArr = env -> NewObjectArray(arguments.size(), luaValueClass, NULL);
        int index = 0;
        for (LuaArgumentList::iterator it = arguments.begin(); it != arguments.end(); it ++)
        {
            LuaValue *argument = *it;
            jobject jArgument = LuaJavaConverter::convertToJavaLuaValueByLuaValue(env, objectClass -> getContext(), argument);
            env -> SetObjectArrayElement(argumentArr, index, jArgument);
            index++;
        }

        jobject result = env -> CallObjectMethod(instance, invokeMethodID, jMethodName, argumentArr);
        retValue = LuaJavaConverter::convertToLuaValueByJLuaValue(env, result);
    }

    LuaJavaEnv::resetEnv(env);

    if (retValue == NULL)
    {
        retValue = new LuaValue();
    }

    return retValue;
}

/**
 * 类获取器处理器
 *
 * @param module 模块对象
 * @param fieldName 字段名称
 *
 * @return 返回值
 */
static LuaValue* _luaClassGetterHandler (cn::vimfung::luascriptcore::modules::oo::LuaObjectClass *objectClass, std::string fieldName)
{
    using namespace cn::vimfung::luascriptcore;

    JNIEnv *env = LuaJavaEnv::getEnv();
    LuaValue *retValue = NULL;

    lua_State *state = objectClass -> getContext() -> getLuaState();
    if (lua_gettop(state) > 0 && lua_isuserdata(state, 1))
    {
        //表示有实例对象传入
        LuaUserdataRef ref = (LuaUserdataRef)lua_touserdata(state, 1);
        jobject instance = (jobject)((LuaObjectDescriptor *)ref -> value) -> getObject();

        jmethodID getFieldId = env -> GetMethodID(LuaJavaType::luaObjectClass(env), "_getField", "(Ljava/lang/String;)Lcn/vimfung/luascriptcore/LuaValue;");

        jstring fieldNameStr = env -> NewStringUTF(fieldName.c_str());
        jobject retObj = env -> CallObjectMethod(instance, getFieldId, fieldNameStr);

        if (retObj != NULL)
        {
            retValue = LuaJavaConverter::convertToLuaValueByJLuaValue(env, retObj);
        }
    }

    LuaJavaEnv::resetEnv(env);

    if (retValue == NULL)
    {
        retValue = new LuaValue();
    }

    return retValue;
}

/**
 * 类设置器处理器
 *
 * @param module 模块对象
 * @param fieldName 字段名称
 * @param value 字段值
 */
static void _luaClassSetterHandler (cn::vimfung::luascriptcore::modules::oo::LuaObjectClass *objectClass, std::string fieldName, LuaValue *value)
{
    using namespace cn::vimfung::luascriptcore;
    JNIEnv *env = LuaJavaEnv::getEnv();

    lua_State *state = objectClass -> getContext() -> getLuaState();

    if (lua_gettop(state) > 0 && lua_isuserdata(state, 1))
    {
        //表示有实例对象传入
        LuaUserdataRef ref = (LuaUserdataRef)lua_touserdata(state, 1);
        jobject instance = (jobject)((LuaObjectDescriptor *)ref -> value) -> getObject();

        jmethodID setFieldId = env -> GetMethodID(LuaJavaType::luaObjectClass(env), "_setField", "(Ljava/lang/String;Lcn/vimfung/luascriptcore/LuaValue;)V");

        jstring fieldNameStr = env -> NewStringUTF(fieldName.c_str());
        env -> CallVoidMethod(instance, setFieldId, fieldNameStr, LuaJavaConverter::convertToJavaLuaValueByLuaValue(env, objectClass -> getContext(), value));
    }

    LuaJavaEnv::resetEnv(env);
}

static void _luaSubclassHandler (cn::vimfung::luascriptcore::modules::oo::LuaObjectClass *objectClass, std::string subclassName)
{
    JNIEnv *env = LuaJavaEnv::getEnv();

    LuaJavaObjectClass *javaObjectClass = (LuaJavaObjectClass *)objectClass;

    //创建子类描述
    LuaJavaObjectClass *subclass = new LuaJavaObjectClass(
            env,
            (const std::string)javaObjectClass -> getName(),
            javaObjectClass -> getModuleClass(env),
            NULL,
            NULL,
            NULL);
    objectClass -> getContext() -> registerModule((const std::string)subclassName, subclass);
    subclass -> release();

    LuaJavaEnv::resetEnv(env);
}

LuaJavaObjectClass::LuaJavaObjectClass(JNIEnv *env,
                                       const std::string &superClassName,
                                       jclass moduleClass,
                                       jobjectArray fields,
                                       jobjectArray instanceMethods,
                                       jobjectArray classMethods)
    : LuaObjectClass(superClassName)
{
    _moduleClass = (jclass)env -> NewWeakGlobalRef(moduleClass);
    _fields = fields;
    _instanceMethods = instanceMethods;
    _classMethods = classMethods;

    this -> onObjectCreated(_luaClassObjectCreated);
    this -> onObjectDestroy(_luaClassObjectDestroy);
    this -> onObjectGetDescription(_luaClassObjectDescription);
    this -> onSubClass(_luaSubclassHandler);
}

jclass LuaJavaObjectClass::getModuleClass(JNIEnv *env)
{
    if (env -> IsSameObject(_moduleClass, NULL) != JNI_TRUE)
    {
        return _moduleClass;
    }

    return NULL;
}

void LuaJavaObjectClass::onRegister(const std::string &name,
                                    cn::vimfung::luascriptcore::LuaContext *context)
{
    cn::vimfung::luascriptcore::modules::oo::LuaObjectClass::onRegister(name, context);

    JNIEnv *env = LuaJavaEnv::getEnv();

    jclass jfieldClass = LuaJavaType::fieldClass(env);
    jmethodID getFieldNameMethodId = env -> GetMethodID(jfieldClass, "getName", "()Ljava/lang/String;");

    jclass jmethodClass = LuaJavaType::methodClass(env);
    jmethodID getMethodNameMethodId = env -> GetMethodID(jmethodClass, "getName", "()Ljava/lang/String;");

    //注册模块字段
    if (_fields != NULL)
    {
        jsize fieldCount = env -> GetArrayLength(_fields);
        for (int i = 0; i < fieldCount; ++i)
        {
            jobject field = env -> GetObjectArrayElement(_fields, i);
            jstring fieldName = (jstring)env -> CallObjectMethod(field, getFieldNameMethodId);

            const char *fieldNameCStr = env -> GetStringUTFChars(fieldName, NULL);
            this -> registerInstanceField(fieldNameCStr, _luaClassGetterHandler, _luaClassSetterHandler);
            env -> ReleaseStringUTFChars(fieldName, fieldNameCStr);
        }
    }

    //注册实例方法
    if (_instanceMethods != NULL)
    {
        jsize methodCount = env -> GetArrayLength(_instanceMethods);
        for (int i = 0; i < methodCount; ++i)
        {
            jobject method = env -> GetObjectArrayElement(_instanceMethods, i);
            jstring methodName = (jstring)env -> CallObjectMethod(method, getMethodNameMethodId);

            const char *methodNameCStr = env -> GetStringUTFChars(methodName, NULL);
            this -> registerInstanceMethod(methodNameCStr, _luaInstanceMethodHandler);
            env -> ReleaseStringUTFChars(methodName, methodNameCStr);
        }
    }

    //注册类方法
    if (_classMethods != NULL)
    {
        jsize classMethodCount = env -> GetArrayLength(_classMethods);
        for (int i = 0; i < classMethodCount; ++i)
        {
            jobject method = env -> GetObjectArrayElement(_classMethods, i);
            jstring methodName = (jstring)env -> CallObjectMethod(method, getMethodNameMethodId);

            const char *methodNameCStr = env -> GetStringUTFChars(methodName, NULL);
            this -> registerMethod(methodNameCStr, _luaClassMethodHandler);
            env -> ReleaseStringUTFChars(methodName, methodNameCStr);
        }
    }

    LuaJavaEnv::resetEnv(env);

}

void LuaJavaObjectClass::createLuaInstance(cn::vimfung::luascriptcore::LuaObjectDescriptor *objectDescriptor)
{
    cn::vimfung::luascriptcore::modules::oo::LuaObjectClass::createLuaInstance(objectDescriptor);

    //关联对象
    JNIEnv *env = LuaJavaEnv::getEnv();
    LuaJavaEnv::associcateInstance(env, (jobject)objectDescriptor -> getObject(), objectDescriptor);
    LuaJavaEnv::resetEnv(env);
}