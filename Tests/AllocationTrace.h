#pragma once
#include <cstdlib>
#include <new>
#ifdef _MSC_VER
#include <malloc.h>
#endif
namespace allocationTrace { inline thread_local bool enabled=false;inline thread_local size_t count=0; }
void* operator new(std::size_t size) {if(allocationTrace::enabled)++allocationTrace::count;if(void* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
#ifdef _MSC_VER
void* operator new(std::size_t size,std::align_val_t alignment){if(allocationTrace::enabled)++allocationTrace::count;if(void* p=_aligned_malloc(size?size:1,size_t(alignment)))return p;throw std::bad_alloc();}
void* operator new[](std::size_t size,std::align_val_t alignment){return ::operator new(size,alignment);}
void operator delete(void* p,std::align_val_t) noexcept{_aligned_free(p);}
void operator delete[](void* p,std::align_val_t) noexcept{_aligned_free(p);}
void operator delete(void* p,std::size_t,std::align_val_t) noexcept{_aligned_free(p);}
void operator delete[](void* p,std::size_t,std::align_val_t) noexcept{_aligned_free(p);}
#endif
