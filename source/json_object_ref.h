#ifndef JSON_OBJECT_REF_H
#define JSON_OBJECT_REF_H

#include <string>
#include <json-c/json.h>

// Owning, scope-bound handle for one json-c reference. Releases that reference
// in the destructor, so every return path in the enclosing scope is covered.
class JsonObjectRef
{
public:
    explicit JsonObjectRef(json_object *obj = nullptr) : obj_(obj) {}

    JsonObjectRef(const JsonObjectRef &) = delete;
    JsonObjectRef &operator=(const JsonObjectRef &) = delete;

    JsonObjectRef(JsonObjectRef &&other) noexcept : obj_(other.obj_)
    {
        other.obj_ = nullptr;
    }

    JsonObjectRef &operator=(JsonObjectRef &&other) noexcept
    {
        if (this != &other)
        {
            reset();
            obj_ = other.obj_;
            other.obj_ = nullptr;
        }
        return *this;
    }

    ~JsonObjectRef() { reset(); }

    json_object *get() const { return obj_; }
    explicit operator bool() const { return obj_ != nullptr; }

    // Relinquish ownership, e.g. when handing the object to a json-c API that
    // takes ownership (json_object_array_add / json_object_object_add).
    json_object *release()
    {
        json_object *tmp = obj_;
        obj_ = nullptr;
        return tmp;
    }

    void reset(json_object *obj = nullptr)
    {
        if (obj_ != nullptr && obj_ != obj)
            json_object_put(obj_);
        obj_ = obj;
    }

private:
    json_object *obj_;
};

// Reads obj[key] as a string, falling back to default_value when obj is NULL or
// the key is absent. json-c returns NULL for both of those, and
// std::string(NULL) is undefined behaviour, so a raw json_object_get_string()
// result must never be fed straight into a std::string.
inline std::string JsonGetString(json_object *obj, const char *key, const std::string &default_value = "")
{
    const char *value = json_object_get_string(json_object_object_get(obj, key));
    return value != nullptr ? std::string(value) : default_value;
}

#endif // JSON_OBJECT_REF_H
