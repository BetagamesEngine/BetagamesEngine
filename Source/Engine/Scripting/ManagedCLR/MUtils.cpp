// Copyright (c) 2012-2021 Wojciech Figat. All rights reserved.

#include "MUtils.h"
#include "MClass.h"
#include "MType.h"
#include "Engine/Core/Log.h"
#include "Engine/Core/Types/DataContainer.h"
#include "Engine/Core/Math/BoundingBox.h"
#include "Engine/Core/Math/BoundingSphere.h"
#include "Engine/Core/Math/Rectangle.h"
#include "Engine/Core/Math/Color.h"
#include "Engine/Core/Math/Vector2.h"
#include "Engine/Core/Math/Vector3.h"
#include "Engine/Core/Math/Vector4.h"
#include "Engine/Core/Math/Quaternion.h"
#include "Engine/Core/Math/Matrix.h"
#include "Engine/Core/Math/Transform.h"
#include "Engine/Core/Math/Ray.h"
#include "Engine/Scripting/Scripting.h"
#include "Engine/Scripting/ScriptingObject.h"
#include "Engine/Scripting/StdTypesContainer.h"
#include "Engine/Utilities/StringConverter.h"
#include "Engine/Content/Asset.h"

#if USE_MONO

struct _MonoType
{
    union
    {
        MonoClass* klass;
        MonoType* type;
        MonoArrayType* array;
        MonoMethodSignature* method;
        MonoGenericParam* generic_param;
        MonoGenericClass* generic_class;
    } data;

    unsigned int attrs : 16;
    MonoTypeEnum type : 8;
    unsigned int has_cmods : 1;
    unsigned int byref : 1;
    unsigned int pinned : 1;
};

StringView MUtils::ToString(MonoString* str)
{
    if (str == nullptr)
        return StringView::Empty;
    return StringView((const Char*)mono_string_chars(str), (int32)mono_string_length(str));
}

StringAnsi MUtils::ToStringAnsi(MonoString* str)
{
    if (str == nullptr)
        return StringAnsi::Empty;
    return StringAnsi((const Char*)mono_string_chars(str), (int32)mono_string_length(str));
}

void MUtils::ToString(MonoString* str, String& result)
{
    if (str)
        result.Set((const Char*)mono_string_chars(str), (int32)mono_string_length(str));
    else
        result.Clear();
}

void MUtils::ToString(MonoString* str, StringView& result)
{
    if (str)
    {
        result = StringView((const Char*)mono_string_chars(str), (int32)mono_string_length(str));
    }
    else
    {
        result = StringView();
    }
}

void MUtils::ToString(MonoString* str, Variant& result)
{
    result.SetString(str ? StringView((const Char*)mono_string_chars(str), (int32)mono_string_length(str)) : StringView::Empty);
}

void MUtils::ToString(MonoString* str, MString& result)
{
    if (str)
        result.Set((const Char*)mono_string_chars(str), (int32)mono_string_length(str));
    else
        result.Clear();
}

MonoString* MUtils::ToString(const char* str)
{
    if (str == nullptr || *str == 0)
        return mono_string_empty(mono_domain_get());
    return mono_string_new(mono_domain_get(), str);
}

MonoString* MUtils::ToString(const StringAnsi& str)
{
    if (str.IsEmpty())
        return mono_string_empty(mono_domain_get());
    return mono_string_new(mono_domain_get(), str.Get());
}

MonoString* MUtils::ToString(const String& str)
{
    if (str.IsEmpty())
        return mono_string_empty(mono_domain_get());
    return mono_string_new_utf16(mono_domain_get(), (const mono_unichar2*)*str, str.Length());
}

MonoString* MUtils::ToString(const String& str, MonoDomain* domain)
{
    if (str.IsEmpty())
        return mono_string_empty(mono_domain_get());
    return mono_string_new_utf16(domain, (const mono_unichar2*)*str, str.Length());
}

MonoString* MUtils::ToString(const StringAnsiView& str)
{
    if (str.IsEmpty())
        return mono_string_empty(mono_domain_get());
    return mono_string_new_len(mono_domain_get(), str.Get(), str.Length());
}

MonoString* MUtils::ToString(const StringView& str)
{
    if (str.IsEmpty())
        return mono_string_empty(mono_domain_get());
    return mono_string_new_utf16(mono_domain_get(), (const mono_unichar2*)*str, str.Length());
}

MonoString* MUtils::ToString(const StringView& str, MonoDomain* domain)
{
    if (str.IsEmpty())
        return mono_string_empty(domain);
    return mono_string_new_utf16(domain, (const mono_unichar2*)*str, str.Length());
}

VariantType MUtils::UnboxVariantType(MonoReflectionType* value)
{
    if (value == nullptr)
        return VariantType(VariantType::Null);
    return MoveTemp(UnboxVariantType(mono_reflection_type_get_type(value)));
}

VariantType MUtils::UnboxVariantType(MonoType* monoType)
{
    const auto& stdTypes = *StdTypesContainer::Instance();
    const auto klass = mono_type_get_class(monoType);

    // Fast type detection for in-built types
    switch (monoType->type)
    {
    case MONO_TYPE_VOID:
        return VariantType(VariantType::Void);
    case MONO_TYPE_BOOLEAN:
        return VariantType(VariantType::Bool);
    case MONO_TYPE_I1:
        return VariantType(VariantType::Int16);
    case MONO_TYPE_U1:
        return VariantType(VariantType::Int16);
    case MONO_TYPE_I2:
        return VariantType(VariantType::Int16);
    case MONO_TYPE_U2:
        return VariantType(VariantType::Uint16);
    case MONO_TYPE_I4:
    case MONO_TYPE_CHAR:
        return VariantType(VariantType::Int);
    case MONO_TYPE_U4:
        return VariantType(VariantType::Uint);
    case MONO_TYPE_I8:
        return VariantType(VariantType::Int64);
    case MONO_TYPE_U8:
        return VariantType(VariantType::Uint64);
    case MONO_TYPE_R4:
        return VariantType(VariantType::Float);
    case MONO_TYPE_R8:
        return VariantType(VariantType::Double);
    case MONO_TYPE_STRING:
        return VariantType(VariantType::String);
    case MONO_TYPE_PTR:
        return VariantType(VariantType::Pointer);
    case MONO_TYPE_VALUETYPE:
        if (klass == stdTypes.GuidClass->GetNative())
            return VariantType(VariantType::Guid);
        if (klass == stdTypes.TypeClass->GetNative())
            return VariantType(VariantType::Typename);
        if (klass == stdTypes.Vector2Class->GetNative())
            return VariantType(VariantType::Vector2);
        if (klass == stdTypes.Vector3Class->GetNative())
            return VariantType(VariantType::Vector3);
        if (klass == stdTypes.Vector4Class->GetNative())
            return VariantType(VariantType::Vector4);
        if (klass == stdTypes.ColorClass->GetNative())
            return VariantType(VariantType::Color);
        if (klass == stdTypes.BoundingBoxClass->GetNative())
            return VariantType(VariantType::BoundingBox);
        if (klass == stdTypes.QuaternionClass->GetNative())
            return VariantType(VariantType::Quaternion);
        if (klass == stdTypes.TransformClass->GetNative())
            return VariantType(VariantType::Transform);
        if (klass == stdTypes.BoundingSphereClass->GetNative())
            return VariantType(VariantType::BoundingSphere);
        if (klass == stdTypes.RectangleClass->GetNative())
            return VariantType(VariantType::Rectangle);
        if (klass == stdTypes.MatrixClass->GetNative())
            return VariantType(VariantType::Matrix);
        break;
    case MONO_TYPE_OBJECT:
        return VariantType(VariantType::ManagedObject);
    case MONO_TYPE_SZARRAY:
        if (klass == mono_array_class_get(mono_get_byte_class(), 1))
            return VariantType(VariantType::Blob);
        break;
    }

    // Get actual typename for full type info
    if (!klass)
        return VariantType(VariantType::Null);
    MString fullname;
    GetClassFullname(klass, fullname);
    switch (monoType->type)
    {
    case MONO_TYPE_SZARRAY:
    case MONO_TYPE_ARRAY:
        return VariantType(VariantType::Array, fullname);
    case MONO_TYPE_ENUM:
        return VariantType(VariantType::Enum, fullname);
    case MONO_TYPE_VALUETYPE:
        return VariantType(VariantType::Structure, fullname);
    }
    if (mono_class_is_subclass_of(klass, Asset::GetStaticClass()->GetNative(), false) != 0)
    {
        if (klass == Asset::GetStaticClass()->GetNative())
            return VariantType(VariantType::Asset);
        return VariantType(VariantType::Asset, fullname);
    }
    if (mono_class_is_subclass_of(klass, ScriptingObject::GetStaticClass()->GetNative(), false) != 0)
    {
        if (klass == ScriptingObject::GetStaticClass()->GetNative())
            return VariantType(VariantType::Object);
        return VariantType(VariantType::Object, fullname);
    }
    // TODO: support any dictionary unboxing

    LOG(Error, "Invalid managed type to unbox {0}", String(fullname));
    return VariantType();
}

MonoReflectionType* MUtils::BoxVariantType(const VariantType& value)
{
    if (value.Type == VariantType::Null)
        return nullptr;
    MonoClass* klass = GetClass(value);
    if (!klass)
    {
        LOG(Error, "Invalid native type to box {0}", value);
        return nullptr;
    }
    MonoType* monoType = mono_class_get_type(klass);
    return mono_type_get_object(mono_domain_get(), monoType);
}

Variant MUtils::UnboxVariant(MonoObject* value)
{
    if (value == nullptr)
        return Variant::Null;

    const auto& stdTypes = *StdTypesContainer::Instance();
    const auto klass = mono_object_get_class(value);

    if (klass == mono_get_boolean_class())
        return *static_cast<bool*>(mono_object_unbox(value));
    if (klass == mono_get_byte_class())
        return (int16)*static_cast<byte*>(mono_object_unbox(value));
    if (klass == mono_get_sbyte_class())
        return (int16)*static_cast<int8*>(mono_object_unbox(value));
    if (klass == mono_get_int16_class())
        return *static_cast<int16*>(mono_object_unbox(value));
    if (klass == mono_get_uint16_class())
        return *static_cast<uint16*>(mono_object_unbox(value));
    if (klass == mono_get_int32_class())
        return *static_cast<int32*>(mono_object_unbox(value));
    if (klass == mono_get_uint32_class())
        return *static_cast<uint32*>(mono_object_unbox(value));
    if (klass == mono_get_int64_class())
        return *static_cast<int64*>(mono_object_unbox(value));
    if (klass == mono_get_uint64_class())
        return *static_cast<uint64*>(mono_object_unbox(value));
    if (klass == mono_get_char_class())
        return *static_cast<Char*>(mono_object_unbox(value));
    if (klass == mono_get_single_class())
        return *static_cast<float*>(mono_object_unbox(value));
    if (klass == mono_get_double_class())
        return *static_cast<double*>(mono_object_unbox(value));
    if (klass == stdTypes.GuidClass->GetNative())
        return Variant(*static_cast<Guid*>(mono_object_unbox(value)));
    if (klass == stdTypes.Vector2Class->GetNative())
        return *static_cast<Vector2*>(mono_object_unbox(value));
    if (klass == stdTypes.Vector3Class->GetNative())
        return *static_cast<Vector3*>(mono_object_unbox(value));
    if (klass == stdTypes.Vector4Class->GetNative())
        return *static_cast<Vector4*>(mono_object_unbox(value));
    if (klass == stdTypes.ColorClass->GetNative())
        return *static_cast<Color*>(mono_object_unbox(value));
    if (klass == mono_get_string_class())
        return Variant(MUtils::ToString((MonoString*)value));
    if (klass == stdTypes.BoundingBoxClass->GetNative())
        return Variant(*static_cast<BoundingBox*>(mono_object_unbox(value)));
    if (klass == stdTypes.QuaternionClass->GetNative())
        return *static_cast<Quaternion*>(mono_object_unbox(value));
    if (klass == stdTypes.TransformClass->GetNative())
        return Variant(*static_cast<Transform*>(mono_object_unbox(value)));
    if (klass == stdTypes.BoundingSphereClass->GetNative())
        return *static_cast<BoundingSphere*>(mono_object_unbox(value));
    if (klass == stdTypes.RectangleClass->GetNative())
        return *static_cast<Rectangle*>(mono_object_unbox(value));
    if (klass == mono_get_intptr_class() || klass == mono_get_uintptr_class())
        return *static_cast<void**>(mono_object_unbox(value));
    if (klass == stdTypes.MatrixClass->GetNative())
        return Variant(*reinterpret_cast<Matrix*>(mono_object_unbox(value)));
    if (mono_class_is_subclass_of(klass, Asset::GetStaticClass()->GetNative(), false) != 0)
        return static_cast<Asset*>(ScriptingObject::ToNative(value));
    if (mono_class_is_subclass_of(klass, ScriptingObject::GetStaticClass()->GetNative(), false) != 0)
        return ScriptingObject::ToNative(value);
    if (mono_class_is_enum(klass))
    {
        MString fullname;
        GetClassFullname(klass, fullname);
        Variant v;
        v.Type = MoveTemp(VariantType(VariantType::Enum, fullname));
        // TODO: what about 64-bit enum? use enum size with memcpy
        v.AsUint64 = *static_cast<uint32*>(mono_object_unbox(value));
        return v;
    }
    if (mono_class_is_valuetype(klass))
    {
        MString fullname;
        GetClassFullname(klass, fullname);
        const ScriptingTypeHandle typeHandle = Scripting::FindScriptingType(fullname);
        if (typeHandle)
        {
            const ScriptingType& type = typeHandle.GetType();
            Variant v;
            v.Type = MoveTemp(VariantType(VariantType::Structure, fullname));
            v.AsBlob.Data = Allocator::Allocate(type.Size);
            v.AsBlob.Length = type.Size;
            type.Struct.Ctor(v.AsBlob.Data);
            type.Struct.Unbox(v.AsBlob.Data, value);
            return v;
        }
        return Variant(value);
    }
    if (klass == mono_array_class_get(mono_get_byte_class(), 1))
    {
        Variant v;
        v.SetBlob(mono_array_addr((MonoArray*)value, byte, 0), (int32)mono_array_length((MonoArray*)value));
        return v;
    }
    MonoType* monoType = mono_class_get_type(klass);
    if (monoType->type == MONO_TYPE_SZARRAY || monoType->type == MONO_TYPE_ARRAY)
    {
        MString fullname;
        GetClassFullname(klass, fullname);
        Variant v;
        v.SetType(MoveTemp(VariantType(VariantType::Array, fullname)));
        auto& array = v.AsArray();
        array.Resize((int32)mono_array_length((MonoArray*)value));
        const StringAnsiView elementTypename(*fullname, fullname.Length() - 2);
        MonoClass* elementClass = monoType->data.array->eklass;
        uint32_t elementAlign;
        const int32 elementSize = mono_class_value_size(elementClass, &elementAlign);
        if (mono_class_is_enum(elementClass))
        {
            // Array of Enums
            for (int32 i = 0; i < array.Count(); i++)
            {
                array[i].SetType(VariantType(VariantType::Enum, elementTypename));
                Platform::MemoryCopy(&array[i].AsUint64, mono_array_addr_with_size((MonoArray*)value, elementSize, i), elementSize);
            }
        }
        else if (mono_class_is_valuetype(elementClass))
        {
            // Array of Structures
            VariantType elementType = UnboxVariantType(mono_class_get_type(elementClass));
            switch (elementType.Type)
            {
            case VariantType::Bool:
            case VariantType::Int:
            case VariantType::Uint:
            case VariantType::Int64:
            case VariantType::Uint64:
            case VariantType::Float:
            case VariantType::Double:
            case VariantType::Vector2:
            case VariantType::Vector3:
            case VariantType::Vector4:
            case VariantType::Color:
            case VariantType::Guid:
            case VariantType::BoundingSphere:
            case VariantType::Quaternion:
            case VariantType::Rectangle:
            case VariantType::Int2:
            case VariantType::Int3:
            case VariantType::Int4:
            case VariantType::Int16:
            case VariantType::Uint16:
                // Optimized unboxing of raw data type
                for (int32 i = 0; i < array.Count(); i++)
                {
                    auto& a = array[i];
                    a.SetType(elementType);
                    Platform::MemoryCopy(&a.AsData, mono_array_addr_with_size((MonoArray*)value, elementSize, i), elementSize);
                }
                break;
            case VariantType::BoundingBox:
            case VariantType::Transform:
            case VariantType::Ray:
            case VariantType::Matrix:
                // Optimized unboxing of raw data type
                for (int32 i = 0; i < array.Count(); i++)
                {
                    auto& a = array[i];
                    a.SetType(elementType);
                    Platform::MemoryCopy(a.AsBlob.Data, mono_array_addr_with_size((MonoArray*)value, elementSize, i), elementSize);
                }
                break;
            case VariantType::Structure:
            {
                const ScriptingTypeHandle typeHandle = Scripting::FindScriptingType(elementType.TypeName);
                if (typeHandle)
                {
                    // Unbox array of structures
                    const ScriptingType& type = typeHandle.GetType();
                    ASSERT(type.Type == ScriptingTypes::Structure);
                    // TODO: optimize this for large arrays to prevent multiple AllocStructure calls in Variant::SetType by using computed struct type
                    for (int32 i = 0; i < array.Count(); i++)
                    {
                        auto& a = array[i];
                        a.SetType(elementType);
                        void* managed = mono_array_addr_with_size((MonoArray*)value, elementSize, i);
                        // TODO: optimize structures unboxing to not require MonoObject* but raw managed value data to prevent additional boxing here
                        MonoObject* boxed = mono_object_new(mono_domain_get(), elementClass);
                        Platform::MemoryCopy(mono_object_unbox(boxed), managed, elementSize);
                        type.Struct.Unbox(a.AsBlob.Data, boxed);
                    }
                    break;
                }
                LOG(Error, "Invalid type to unbox {0}", v.Type);
                break;
            }
            default:
                LOG(Error, "Invalid type to unbox {0}", v.Type);
                break;
            }
        }
        else
        {
            // Array of Objects
            for (int32 i = 0; i < array.Count(); i++)
                array[i] = UnboxVariant(mono_array_get((MonoArray*)value, MonoObject*, i));
        }
        return v;
    }
    // TODO: support any dictionary unboxing

    return Variant(value);
}

MonoObject* MUtils::BoxVariant(const Variant& value)
{
    const auto& stdTypes = *StdTypesContainer::Instance();
    switch (value.Type.Type)
    {
    case VariantType::Null:
    case VariantType::Void:
        return nullptr;
    case VariantType::Bool:
        return mono_value_box(mono_domain_get(), mono_get_boolean_class(), (void*)&value.AsBool);
    case VariantType::Int16:
        return mono_value_box(mono_domain_get(), mono_get_int16_class(), (void*)&value.AsInt16);
    case VariantType::Uint16:
        return mono_value_box(mono_domain_get(), mono_get_uint16_class(), (void*)&value.AsUint16);
    case VariantType::Int:
        return mono_value_box(mono_domain_get(), mono_get_int32_class(), (void*)&value.AsInt);
    case VariantType::Uint:
        return mono_value_box(mono_domain_get(), mono_get_uint32_class(), (void*)&value.AsUint);
    case VariantType::Int64:
        return mono_value_box(mono_domain_get(), mono_get_int64_class(), (void*)&value.AsInt64);
    case VariantType::Uint64:
        return mono_value_box(mono_domain_get(), mono_get_uint64_class(), (void*)&value.AsUint64);
    case VariantType::Float:
        return mono_value_box(mono_domain_get(), mono_get_single_class(), (void*)&value.AsFloat);
    case VariantType::Double:
        return mono_value_box(mono_domain_get(), mono_get_double_class(), (void*)&value.AsDouble);
    case VariantType::Vector2:
        return mono_value_box(mono_domain_get(), stdTypes.Vector2Class->GetNative(), (void*)&value.AsData);
    case VariantType::Vector3:
        return mono_value_box(mono_domain_get(), stdTypes.Vector3Class->GetNative(), (void*)&value.AsData);
    case VariantType::Vector4:
        return mono_value_box(mono_domain_get(), stdTypes.Vector4Class->GetNative(), (void*)&value.AsData);
    case VariantType::Color:
        return mono_value_box(mono_domain_get(), stdTypes.ColorClass->GetNative(), (void*)&value.AsData);
    case VariantType::Guid:
        return mono_value_box(mono_domain_get(), stdTypes.GuidClass->GetNative(), (void*)&value.AsData);
    case VariantType::String:
        return (MonoObject*)MUtils::ToString((StringView)value);
    case VariantType::Quaternion:
        return mono_value_box(mono_domain_get(), stdTypes.QuaternionClass->GetNative(), (void*)&value.AsData);
    case VariantType::BoundingSphere:
        return mono_value_box(mono_domain_get(), stdTypes.BoundingSphereClass->GetNative(), (void*)&value.AsData);
    case VariantType::Rectangle:
        return mono_value_box(mono_domain_get(), stdTypes.RectangleClass->GetNative(), (void*)&value.AsData);
    case VariantType::Pointer:
        return mono_value_box(mono_domain_get(), mono_get_intptr_class(), (void*)&value.AsPointer);
    case VariantType::Ray:
        return mono_value_box(mono_domain_get(), stdTypes.RayClass->GetNative(), value.AsBlob.Data);
    case VariantType::BoundingBox:
        return mono_value_box(mono_domain_get(), stdTypes.BoundingBoxClass->GetNative(), value.AsBlob.Data);
    case VariantType::Transform:
        return mono_value_box(mono_domain_get(), stdTypes.TransformClass->GetNative(), value.AsBlob.Data);
    case VariantType::Matrix:
        return mono_value_box(mono_domain_get(), stdTypes.MatrixClass->GetNative(), value.AsBlob.Data);
    case VariantType::Blob:
        return (MonoObject*)ToArray(Span<byte>((const byte*)value.AsBlob.Data, value.AsBlob.Length));
    case VariantType::Object:
        return value.AsObject ? value.AsObject->GetOrCreateManagedInstance() : nullptr;
    case VariantType::Asset:
        return value.AsAsset ? value.AsAsset->GetOrCreateManagedInstance() : nullptr;
    case VariantType::Array:
    {
        MonoArray* managed;
        const auto& array = value.AsArray();
        if (value.Type.TypeName)
        {
            const StringAnsiView elementTypename(value.Type.TypeName, StringUtils::Length(value.Type.TypeName) - 2);
            const ScriptingTypeHandle typeHandle = Scripting::FindScriptingType(elementTypename);
            MonoClass* elementClass;
            if (typeHandle && typeHandle.GetType().ManagedClass)
                elementClass = typeHandle.GetType().ManagedClass->GetNative();
            else
                elementClass = Scripting::FindClassNative(elementTypename);
            if (!elementClass)
            {
                LOG(Error, "Invalid type to box {0}", value.Type);
                return nullptr;
            }
            uint32_t elementAlign;
            const int32 elementSize = mono_class_value_size(elementClass, &elementAlign);
            managed = mono_array_new(mono_domain_get(), elementClass, array.Count());
            if (mono_class_is_enum(elementClass))
            {
                // Array of Enums
                for (int32 i = 0; i < array.Count(); i++)
                {
                    auto data = (uint64)array[i];
                    Platform::MemoryCopy(mono_array_addr_with_size(managed, elementSize, i), &data, elementSize);
                }
            }
            else if (mono_class_is_valuetype(elementClass))
            {
                // Array of Structures
                VariantType elementType = UnboxVariantType(mono_class_get_type(elementClass));
                switch (elementType.Type)
                {
                case VariantType::Bool:
                case VariantType::Int:
                case VariantType::Uint:
                case VariantType::Int64:
                case VariantType::Uint64:
                case VariantType::Float:
                case VariantType::Double:
                case VariantType::Vector2:
                case VariantType::Vector3:
                case VariantType::Vector4:
                case VariantType::Color:
                case VariantType::Guid:
                case VariantType::BoundingSphere:
                case VariantType::Quaternion:
                case VariantType::Rectangle:
                case VariantType::Int2:
                case VariantType::Int3:
                case VariantType::Int4:
                case VariantType::Int16:
                case VariantType::Uint16:
                    // Optimized boxing of raw data type
                    for (int32 i = 0; i < array.Count(); i++)
                        Platform::MemoryCopy(mono_array_addr_with_size(managed, elementSize, i), &array[i].AsData, elementSize);
                    break;
                case VariantType::BoundingBox:
                case VariantType::Transform:
                case VariantType::Ray:
                case VariantType::Matrix:
                    // Optimized boxing of raw data type
                    for (int32 i = 0; i < array.Count(); i++)
                        Platform::MemoryCopy(mono_array_addr_with_size(managed, elementSize, i), array[i].AsBlob.Data, elementSize);
                    break;
                case VariantType::Structure:
                    if (typeHandle)
                    {
                        const ScriptingType& type = typeHandle.GetType();
                        ASSERT(type.Type == ScriptingTypes::Structure);
                        for (int32 i = 0; i < array.Count(); i++)
                        {
                            // TODO: optimize structures boxing to not return MonoObject* but use raw managed object to prevent additional boxing here
                            MonoObject* boxed = type.Struct.Box(array[i].AsBlob.Data);
                            Platform::MemoryCopy(mono_array_addr_with_size(managed, elementSize, i), mono_object_unbox(boxed), elementSize);
                        }
                        break;
                    }
                    LOG(Error, "Invalid type to box {0}", value.Type);
                    break;
                default:
                    LOG(Error, "Invalid type to box {0}", value.Type);
                    break;
                }
            }
            else
            {
                // Array of Objects
                for (int32 i = 0; i < array.Count(); i++)
                    mono_array_setref(managed, i, BoxVariant(array[i]));
            }
        }
        else
        {
            // object[]
            managed = mono_array_new(mono_domain_get(), mono_get_object_class(), array.Count());
            for (int32 i = 0; i < array.Count(); i++)
                mono_array_setref(managed, i, BoxVariant(array[i]));
        }
        return (MonoObject*)managed;
    }
        // TODO: VariantType::Dictionary
    case VariantType::Structure:
    {
        if (value.AsBlob.Data == nullptr)
            return nullptr;
        const ScriptingTypeHandle typeHandle = Scripting::FindScriptingType(StringAnsiView(value.Type.TypeName));
        if (typeHandle)
        {
            const ScriptingType& type = typeHandle.GetType();
            return type.Struct.Box(value.AsBlob.Data);
        }
        LOG(Error, "Invalid type to box {0}", value.Type);
        return nullptr;
    }
    case VariantType::Enum:
    {
        const auto klass = Scripting::FindClassNative(StringAnsiView(value.Type.TypeName));
        if (klass)
            return mono_value_box(mono_domain_get(), klass, (void*)&value.AsUint64);
        LOG(Error, "Invalid type to box {0}", value.Type);
        return nullptr;
    }
    case VariantType::ManagedObject:
        return value.AsUint ? mono_gchandle_get_target(value.AsUint) : nullptr;
    case VariantType::Typename:
    {
        const auto klass = Scripting::FindClassNative((StringAnsiView)value);
        if (klass)
            return (MonoObject*)GetType(klass);
        LOG(Error, "Invalid type to box {0}", value);
        return nullptr;
    }
    default:
        LOG(Error, "Invalid type to box {0}", value.Type);
        return nullptr;
    }
}

void MUtils::GetClassFullname(MonoObject* obj, MString& fullname)
{
    if (obj == nullptr)
        return;

    MonoClass* monoClass = mono_object_get_class(obj);
    GetClassFullname(monoClass, fullname);
}

void MUtils::GetClassFullname(MonoClass* monoClass, MString& fullname)
{
    static MString plusStr("+");
    static MString dotStr(".");
    MonoClass* nestingClass = mono_class_get_nesting_type(monoClass);
    MonoClass* lastClass = monoClass;

    fullname = mono_class_get_name(monoClass);

    while (nestingClass)
    {
        lastClass = nestingClass;
        fullname = mono_class_get_name(nestingClass) + plusStr + fullname;
        nestingClass = mono_class_get_nesting_type(nestingClass);
    }

    const char* lastClassNamespace = mono_class_get_namespace(lastClass);
    if (lastClassNamespace && *lastClassNamespace)
        fullname = lastClassNamespace + dotStr + fullname;
}

void MUtils::GetClassFullname(MonoReflectionType* type, MString& fullname)
{
    if (!type)
        return;
    MonoType* monoType = mono_reflection_type_get_type(type);
    MonoClass* monoClass = mono_type_get_class(monoType);
    GetClassFullname(monoClass, fullname);
}

MonoClass* MUtils::GetClass(MonoObject* object)
{
    return mono_object_get_class(object);
}

MonoClass* MUtils::GetClass(MonoReflectionType* type)
{
    if (!type)
        return nullptr;
    MonoType* monoType = mono_reflection_type_get_type(type);
    return mono_type_get_class(monoType);
}

MonoClass* MUtils::GetClass(const VariantType& value)
{
    const auto& stdTypes = *StdTypesContainer::Instance();
    auto mclass = Scripting::FindClass(StringAnsiView(value.TypeName));
    if (mclass)
        return mclass->GetNative();
    switch (value.Type)
    {
    case VariantType::Void:
        return mono_get_void_class();
    case VariantType::Bool:
        return mono_get_boolean_class();
    case VariantType::Int16:
        return mono_get_int16_class();
    case VariantType::Uint16:
        return mono_get_uint16_class();
    case VariantType::Int:
        return mono_get_int32_class();
    case VariantType::Uint:
        return mono_get_uint32_class();
    case VariantType::Int64:
        return mono_get_int64_class();
    case VariantType::Uint64:
        return mono_get_uint64_class();
    case VariantType::Float:
        return mono_get_single_class();
    case VariantType::Double:
        return mono_get_double_class();
    case VariantType::Pointer:
        return mono_get_intptr_class();
    case VariantType::String:
        return mono_get_string_class();
    case VariantType::Object:
        return ScriptingObject::GetStaticClass()->GetNative();
    case VariantType::Asset:
        return Asset::GetStaticClass()->GetNative();
    case VariantType::Blob:
        return mono_array_class_get(mono_get_byte_class(), 1);
    case VariantType::Vector2:
        return stdTypes.Vector2Class->GetNative();
    case VariantType::Vector3:
        return stdTypes.Vector3Class->GetNative();
    case VariantType::Vector4:
        return stdTypes.Vector4Class->GetNative();
    case VariantType::Color:
        return stdTypes.ColorClass->GetNative();
    case VariantType::Guid:
        return stdTypes.GuidClass->GetNative();
    case VariantType::Typename:
        return stdTypes.TypeClass->GetNative();
    case VariantType::BoundingBox:
        return stdTypes.BoundingBoxClass->GetNative();
    case VariantType::BoundingSphere:
        return stdTypes.BoundingSphereClass->GetNative();
    case VariantType::Quaternion:
        return stdTypes.QuaternionClass->GetNative();
    case VariantType::Transform:
        return stdTypes.TransformClass->GetNative();
    case VariantType::Rectangle:
        return stdTypes.RectangleClass->GetNative();
    case VariantType::Ray:
        return stdTypes.RayClass->GetNative();
    case VariantType::Matrix:
        return stdTypes.MatrixClass->GetNative();
    case VariantType::Array:
        if (value.TypeName)
        {
            const StringAnsiView elementTypename(value.TypeName, StringUtils::Length(value.TypeName) - 2);
            mclass = Scripting::FindClass(elementTypename);
            if (mclass)
                return mono_array_class_get(mclass->GetNative(), 1);
        }
        return mono_array_class_get(mono_get_object_class(), 1);
    case VariantType::ManagedObject:
        return mono_get_object_class();
    default: ;
    }
    return nullptr;
}

MonoClass* MUtils::GetClass(const Variant& value)
{
    const auto& stdTypes = *StdTypesContainer::Instance();
    switch (value.Type.Type)
    {
    case VariantType::Void:
        return mono_get_void_class();
    case VariantType::Bool:
        return mono_get_boolean_class();
    case VariantType::Int16:
        return mono_get_int16_class();
    case VariantType::Uint16:
        return mono_get_uint16_class();
    case VariantType::Int:
        return mono_get_int32_class();
    case VariantType::Uint:
        return mono_get_uint32_class();
    case VariantType::Int64:
        return mono_get_int64_class();
    case VariantType::Uint64:
        return mono_get_uint64_class();
    case VariantType::Float:
        return mono_get_single_class();
    case VariantType::Double:
        return mono_get_double_class();
    case VariantType::Pointer:
        return mono_get_intptr_class();
    case VariantType::String:
        return mono_get_string_class();
    case VariantType::Blob:
        return mono_array_class_get(mono_get_byte_class(), 1);
    case VariantType::Vector2:
        return stdTypes.Vector2Class->GetNative();
    case VariantType::Vector3:
        return stdTypes.Vector3Class->GetNative();
    case VariantType::Vector4:
        return stdTypes.Vector4Class->GetNative();
    case VariantType::Color:
        return stdTypes.ColorClass->GetNative();
    case VariantType::Guid:
        return stdTypes.GuidClass->GetNative();
    case VariantType::Typename:
        return stdTypes.TypeClass->GetNative();
    case VariantType::BoundingBox:
        return stdTypes.BoundingBoxClass->GetNative();
    case VariantType::BoundingSphere:
        return stdTypes.BoundingSphereClass->GetNative();
    case VariantType::Quaternion:
        return stdTypes.QuaternionClass->GetNative();
    case VariantType::Transform:
        return stdTypes.TransformClass->GetNative();
    case VariantType::Rectangle:
        return stdTypes.RectangleClass->GetNative();
    case VariantType::Ray:
        return stdTypes.RayClass->GetNative();
    case VariantType::Matrix:
        return stdTypes.MatrixClass->GetNative();
    case VariantType::Array:
    case VariantType::Dictionary:
        return GetClass(value.Type);
    case VariantType::Object:
        return value.AsObject ? value.AsObject->GetClass()->GetNative() : nullptr;
    case VariantType::Asset:
        return value.AsAsset ? value.AsAsset->GetClass()->GetNative() : nullptr;
    case VariantType::Structure:
    case VariantType::Enum:
        return Scripting::FindClassNative(StringAnsiView(value.Type.TypeName));
    case VariantType::ManagedObject:
        return GetClass((MonoObject*)value);
    default: ;
    }
    return nullptr;
}

MonoReflectionType* MUtils::GetType(MonoObject* object)
{
    MonoClass* klass = GetClass(object);
    return GetType(klass);
}

MonoReflectionType* MUtils::GetType(MonoClass* klass)
{
    MonoType* monoType = mono_class_get_type(klass);
    return mono_type_get_object(mono_domain_get(), monoType);
}

MonoReflectionType* MUtils::GetType(MClass* mclass)
{
    return mclass ? GetType(mclass->GetNative()) : nullptr;
}

BytesContainer MUtils::LinkArray(MonoArray* arrayObj)
{
    BytesContainer result;
    const int32 length = arrayObj ? (int32)mono_array_length(arrayObj) : 0;
    if (length != 0)
    {
        result.Link((byte*)mono_array_addr_with_size(arrayObj, sizeof(byte), 0), length);
    }
    return result;
}

void* MUtils::VariantToManagedArgPtr(Variant& value, const MType& type, bool& failed)
{
    // Convert Variant into matching managed type and return pointer to data for the method invocation
    switch (type.GetNative()->type)
    {
    case MONO_TYPE_BOOLEAN:
        if (value.Type.Type != VariantType::Bool)
            value = (bool)value;
        return &value.AsBool;
    case MONO_TYPE_CHAR:
    case MONO_TYPE_I1:
    case MONO_TYPE_I2:
        if (value.Type.Type != VariantType::Int16)
            value = (int16)value;
        return &value.AsInt16;
    case MONO_TYPE_I4:
        if (value.Type.Type != VariantType::Int)
            value = (int32)value;
        return &value.AsInt;
    case MONO_TYPE_U1:
    case MONO_TYPE_U2:
        if (value.Type.Type != VariantType::Uint16)
            value = (uint16)value;
        return &value.AsUint16;
    case MONO_TYPE_U4:
        if (value.Type.Type != VariantType::Uint)
            value = (uint32)value;
        return &value.AsUint;
    case MONO_TYPE_I8:
        if (value.Type.Type != VariantType::Int64)
            value = (int64)value;
        return &value.AsInt64;
    case MONO_TYPE_U8:
        if (value.Type.Type != VariantType::Uint64)
            value = (uint64)value;
        return &value.AsUint64;
    case MONO_TYPE_R4:
        if (value.Type.Type != VariantType::Float)
            value = (float)value;
        return &value.AsFloat;
    case MONO_TYPE_R8:
        if (value.Type.Type != VariantType::Double)
            value = (double)value;
        return &value.AsDouble;
    case MONO_TYPE_STRING:
        return MUtils::ToString((StringView)value);
    case MONO_TYPE_VALUETYPE:
    {
        MonoClass* klass = type.GetNative()->data.klass;
        if (mono_class_is_enum(klass))
        {
            if (value.Type.Type != VariantType::Enum)
            {
                value.SetType(VariantType(VariantType::Enum, klass));
                value.AsUint64 = 0;
            }
            return &value.AsUint64;
        }
        const auto stdTypes = StdTypesContainer::Instance();
#define CASE_IN_BUILD_TYPE(type) \
    if (klass == stdTypes->type##Class->GetNative()) \
    { \
        if (value.Type.Type != VariantType::type) \
            value = (type)value; \
        return value.AsData; \
    }
        CASE_IN_BUILD_TYPE(Vector2);
        CASE_IN_BUILD_TYPE(Vector3);
        CASE_IN_BUILD_TYPE(Vector4);
        CASE_IN_BUILD_TYPE(Color);
        CASE_IN_BUILD_TYPE(Quaternion);
        CASE_IN_BUILD_TYPE(Guid);
        CASE_IN_BUILD_TYPE(Rectangle);
        CASE_IN_BUILD_TYPE(BoundingSphere);
#undef CASE_IN_BUILD_TYPE
#define CASE_IN_BUILD_TYPE(type) \
    if (klass == stdTypes->type##Class->GetNative()) \
    { \
        if (value.Type.Type != VariantType::type) \
            value = Variant((type)value); \
        return value.AsBlob.Data; \
    }
        CASE_IN_BUILD_TYPE(Matrix);
        CASE_IN_BUILD_TYPE(BoundingBox);
        CASE_IN_BUILD_TYPE(Transform);
        CASE_IN_BUILD_TYPE(Ray);
#undef CASE_IN_BUILD_TYPE
        if (mono_class_is_valuetype(klass))
        {
            if (value.Type.Type == VariantType::Structure)
            {
                const ScriptingTypeHandle typeHandle = Scripting::FindScriptingType(StringAnsiView(value.Type.TypeName));
                if (typeHandle && value.AsBlob.Data)
                {
                    auto& valueType = typeHandle.GetType();
                    if (valueType.ManagedClass->GetNative() == mono_type_get_class(type.GetNative()))
                    {
                        return mono_object_unbox(valueType.Struct.Box(value.AsBlob.Data));
                    }
                    LOG(Error, "Cannot marshal argument of type {0} as {1}", String(valueType.Fullname), type.ToString());
                }
            }
            else
            {
                MString typeName;
                GetClassFullname(klass, typeName);
                const ScriptingTypeHandle typeHandle = Scripting::FindScriptingType(typeName);
                if (typeHandle)
                {
                    auto& valueType = typeHandle.GetType();
                    value.SetType(VariantType(VariantType::Structure, typeName));
                    return mono_object_unbox(valueType.Struct.Box(value.AsBlob.Data));
                }
            }
        }
    }
    break;
    case MONO_TYPE_CLASS:
    {
        if (value.Type.Type == VariantType::Null)
            return nullptr;
        MonoObject* object = BoxVariant(value);
        if (object && !mono_class_is_subclass_of(mono_object_get_class(object), type.GetNative()->data.klass, false))
            object = nullptr;
        return object;
    }
    case MONO_TYPE_OBJECT:
        return BoxVariant(value);
    case MONO_TYPE_SZARRAY:
    case MONO_TYPE_ARRAY:
    {
        if (value.Type.Type != VariantType::Array)
            return nullptr;
        MonoObject* object = BoxVariant(value);
        if (object && !mono_class_is_subclass_of(mono_object_get_class(object), mono_array_class_get(type.GetNative()->data.klass, 1), false))
            object = nullptr;
        return object;
    }
    default:
        break;
    }
    failed = true;
    return nullptr;
}

#endif
