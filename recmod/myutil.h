// Copyright (c) 2000, mamaich
//
// Small container template library. Version 1.1
// Comments are in Russian CP-1251
//
// Compiles under BC 3.1, GCC 2.95, BC 5.x, Watcom C++, VC 6
//---------------------------------------------------------------------------

#ifndef myutilH
#define myutilH

#include <malloc.h>
#include <string.h>

//
// Классики для всякой мути... контейнеры
//

//////////////////////////////////////////////////////////////////////////////
//
//                                    КЛАССЫ
//
//////////////////////////////////////////////////////////////////////////////


// чтоб небыло конфликтов с чужими классами

#ifdef __BCPLUSPLUS__
# if __BCPLUSPLUS__ >= 0x400
namespace Sasha
{
# else
typedef int bool;
#pragma warn -inl
#define throw
# endif
#else
namespace Sasha
{
#endif

#ifdef __WATCOMC__
template <class T>
bool operator==(const T&, const T&);
#endif

//
// TList. Просто шаблонный динамический массивчик.
// Хранит элементы класса T. Сам выделяет и освобождает память.
// Непригоден для очень больших объемов, т.к. выделяет
// память для каждого элемента отдельно.
// Надеюсь, правильно вызывает деструкторы.
//
// Функции
//  Add(T &element) - добавить
//  Delete(int nomer)  - убрать #nomer
//  Empty() - true == пустой
//  Clear() - очистить
//  T& [int nomer] - вернуть эл-т #nomer (не проверяет значение nomer)
//
// Хранимые классы/обьекты должны ОБЯЗАТЕЛЬНО иметь:
//  Конструктор простой
//  Конструктор для копирования
//  оператор ==

template <class T> class TList
{
    int Size;
public:
    T **Items;
    int Count;
    TList()
    {
      Count=0;
      Size=1; Items=(T**)malloc(Size*sizeof(T*));
    }
    TList(const TList &t)
    {
      Count=0;
      Size=t.Size; Items=(T**)malloc(Size*sizeof(T*));
      for(int i=0; i<t.Count; i++)
        Add(t[i]);
    }
    TList &operator=(const TList &t)
    {
      Clear();
      free(Items);
      Count=0;
      Size=t.Size; Items=(T**)malloc(Size*sizeof(T*));
      for(int i=0; i<t.Count; i++)
        Add(t[i]);
      return *this;  
    }
    void Add(const T &w)
    {
      Items[Count]=new T(w);
      Count++;
      if(Count==Size)
      {
          Size+=64;
          Items=(T**)realloc(Items,Size*(sizeof(T*)));
      }
    }
    void Delete(int Who)
    {
      if(Who>=Count||Who<0)
          return;
      delete Items[Who];
      memmove(&Items[Who],&Items[Who+1],(Count-Who-1)*sizeof(T*));
// ^^^ сохранить порядок следования значений, если не надо, то быстрее:
//    if(Count)
//      Items[Who]=Items[Count-1];
      Count--;
      if(Size-Count>128)
      {
        Size=Count+32;
        Items=(T**)realloc(Items,Size*(sizeof(T*)));
      }
    }
    int Find(const T & Who)
    {
        for (int i=0; i<Count; i++)
            if((*Items[i])==Who)
                return i;
        return -1;
    }
    void Remove(const T & Who)
    {
    	int t=Find(Who);
    	if(t>=0)
    		Delete(t);
    }
    int IndexOf(const T & Who) {return Find(Who);}
    bool Contains(const T & Who) {return Find(Who)!=-1;}
    int FindFrom(int Pos, const T & Who)
    {
        for (int i=Pos; i<Count; i++)
            if((*Items[i])==Who)
                return i;
        return -1;
    }
    void Clear()
    {
        for (int i=0; i<Count; i++)
            delete Items[i];
        Count=0;
    }
    ~TList()
    {
      Clear();
      free(Items);
    }
    bool IsEmpty() {return !Count;}
    T& operator[](int V) const
    {
        return *Items[V];
    }
    T& Last()
    {
        if(IsEmpty())
            throw "Sasha::TList - taking last value from empty list!!!";
        return (*this)[Count-1];
    }
};

//
// TStack. Стек. Порожден от TList
//
// Функции
//  Push(T &w)
//  Pop()

template <class T> class TStack : protected TList <T>
{
public:
    TList<T>::Count;

    TStack () : TList<T>() {}
    TStack (TStack &s) : TList<T> (s) {}

    bool IsEmpty() {return !Count;}

    void Push(const T &w) {Add(w);}
    T Pop()
    {
        if(!IsEmpty())
        {
            T t=(*this)[Count-1];
            Delete(Count-1);
            return t;
        } else
            throw "Sasha::TStack - popping from empty stack!!!";
    }
    T& Last()
    {
        if(IsEmpty())
            throw "Sasha::TStack - taking last value from empty stack!!!";
        return (*this)[Count-1];
    }
};


//---------------------------------------------------------------------------
//
// Внимание! Этот контейнер не вызывает конструкторы/деструкторы для хранящихся
// в нем элементов!!! Вызывается только operator= при добавлении нового элемента.
// Зато он работает быстрее и ест меньше памяти чем TList-ы
//
// Warning! This container does not call constructors/destructors for the 
// contained elements!!! Only operator= is called on adding.
// But it consumes much less memory & is faster then TLists.
// It is used to contain basic types or classes which don't allocate/free
// memory in constructors/destructors and don't contain nested classes.
//
template <class T> class TVector
{
    int Size;
public:
    T *Items;
    int Count;
    TVector()
    {
      Count=0;
      Size=1; Items=(T*)malloc(Size*sizeof(T));
    }
    TVector(const TVector &t)
    {
      Count=0;
      Size=t.Size; Items=(T*)malloc(Size*sizeof(T));
      for(int i=0; i<t.Count; i++)
        Add(t[i]);
    }
    void Add(const T &w)
    {
      Items[Count++]=w;
      if(Count==Size)
      {
          Size+=64;
          Items=(T*)realloc(Items,Size*(sizeof(T)));
      }
    }
    void Delete(int Who)
    {
      if(Who>=Count||Who<0)
          return;
      memmove(&Items[Who],&Items[Who+1],(Count-Who-1)*sizeof(T));
// ^^^ сохранить порядок следования значений
//    if(Count)
//      Items[Who]=Items[Count-1];
      Count--;
      if(Size-Count>128)
      {
        Size=Count+1;
        Items=(T*)realloc(Items,Size*(sizeof(T)));
      }
    }
    void Clear()
    {
      Count=0;
      Size=32;
      Items=(T*)realloc(Items,Size*(sizeof(T)));
    }
    ~TVector()
    {
      free(Items);
    }
    T& operator[](int V)
    {
        return Items[V];
    }
    int Find(const T & Who)
    {
        for (int i=0; i<Count; i++)
            if((Items[i])==Who)
                return i;
        return -1;
    }
    int IndexOf(const T & Who) {return Find(Who);}
    bool Contains(const T & Who) {return Find(Who)!=-1;}
    int FindFrom(int Pos, const T & Who)
    {
        for (int i=Pos; i<Count; i++)
            if((Items[i])==Who)
                return i;
        return -1;
    }
};

// T должен содержать operator<
template <class T> class TSortedList
{
public:
    int Count;
protected:    
    T **Items;
    void AddAt(int Pos,T* w)
    {
    // вставляет w после позиции Pos.
    // чтобы вставить в начало: Pos=-1
        if(Count+2>=Size)
        {
            Size+=64;
            Items=(T**)realloc(Items,Size*(sizeof(T*)));
        }
        if(Count-Pos-1>0)
            memmove(&Items[Pos+2],&Items[Pos+1],(Count-Pos-1)*sizeof(T*));
        Items[Pos+1]=w;
        Count++;
    }
    int Size;
public:
    TSortedList()
    {
      Count=0;
      Size=1; Items=(T**)malloc(Size*sizeof(T*));
    }
    int FindPosition(T &w)
    {
        if(Count==0||w<*Items[0])
            return -1;
        if(!(w<*Items[Count-1]))
            return Count-1;
        if(Count==1)
            return 0;
        int Start=0;
        int End=Count-2;
        while(true)
        {
            int Current=(Start+End)/2;
            if(!(w<*Items[Current])&&w<*Items[Current+1])	// !< is >=
                return Current;
            if(End==Start)
                return Current;
            if(w<*Items[Current])
                End=Current;
            else
            {
                if(Start==Current&&End>Start)
                    Start=Current+1;
                else
                    Start=Current;
            }
        }
//        return -1;
    }
    void Add(T &w)
    {
        AddAt(FindPosition(w),new T(w));
    }
    void Delete(int Who)
    {
      if(Who>=Count||Who<0)
          return;
      delete Items[Who];
      memmove(&Items[Who],&Items[Who+1],(Count-Who-1)*sizeof(T*));
      Count--;
      if(Size-Count>128)
      {
        Size=Count+1;
        Items=(T**)realloc(Items,Size*(sizeof(T*)));
      }
    }
    void Clear()
    {
      for(int i=0; i<Count; i++)
          delete Items[i];
      Count=0;
      Size=32;
      Items=(T**)realloc(Items,Size*(sizeof(T*)));
    }
    ~TSortedList()
    {
      Clear();
      free(Items);
    }
    T& operator[](int V)
    {
        return *Items[V];
    }
    TSortedList(const TSortedList &t)
    {
      Count=0;
      Size=t.Size; Items=(T**)malloc(Size*sizeof(T*));
      for(int i=0; i<t.Count; i++)
        Items[i]=new T(*t.Items[i]);
    }
    TSortedList &operator=(const TSortedList &t)
    {
      Clear();
      free(Items);
      Count=t.Count;
      Size=t.Size; Items=(T**)malloc(Size*sizeof(T*));
      for(int i=0; i<t.Count; i++)
        Items[i]=new T(*t.Items[i]);
      return *this;  
    }
};


#ifdef __BCPLUSPLUS__
# if __BCPLUSPLUS__ >= 0x400
};   // namespace Sasha
# endif
#else
};   // namespace Sasha
#endif

#ifdef _BS_DRIVER
using namespace Sasha;
#endif

#endif //#ifndef myutilH
