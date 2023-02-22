#ifndef ARRAY_H
#define ARRAY_H

#define ARRAY_INITIAL_CAPACITY 256

// note: An array whose length can dynamically change at run-time
template <typename T>
struct Array {
    T& operator[](int Index) {
        return Data[Index];
    }

    T* Data;
    size_t Capacity;
    size_t Index;
};

template <typename T>
inline Array<T> array_init(size_t Capacity = ARRAY_INITIAL_CAPACITY) {
    Array<T> Result;

    Result.Data = (T*)malloc(Capacity * sizeof(T));
    Result.Capacity = Capacity;
    Result.Index = 0;

    return Result;
}

template <typename T>
inline void array_add(Array<T>* _Array, T Value) {
    if (_Array->Index >= _Array->Capacity) {
        _Array->Capacity *= 2;
        _Array->Data = (T*)realloc(_Array->Data, _Array->Capacity * sizeof(T));
    }

    _Array->Data[_Array->Index] = Value;
    _Array->Index++;
}

#endif