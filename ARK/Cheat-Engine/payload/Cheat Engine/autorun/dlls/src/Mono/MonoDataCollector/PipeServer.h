#pragma once

#include <Pipe.h>
#include <setjmp.h>
#ifndef _WINDOWS
#include "Metadata.h"
#endif


//#define CUSTOM_DEBUG
#ifdef CUSTOM_DEBUG
#define DEBUG_CONSOLE 1
#endif // CUSTOM_DEBUG

                                //yyyymmdd  (update each time the protocol gets updated)
#define MONO_DATACOLLECTORVERSION 28022025

#define MONO_TYPE_NAME_FORMAT_IL  0
#define MONO_TYPE_NAME_FORMAT_REFLECTION  1
#define MONO_TYPE_NAME_FORMAT_FULL_NAME  2
#define MONO_TYPE_NAME_FORMAT_ASSEMBLY_QUALIFIED  3

#define MONOCMD_ISMONOLOADED 0
#define MONOCMD_OBJECT_GETCLASS 1
#define MONOCMD_ENUMDOMAINS 2
#define MONOCMD_SETCURRENTDOMAIN 3
#define MONOCMD_ENUMASSEMBLIES 4
#define MONOCMD_GETIMAGEFROMASSEMBLY 5
#define MONOCMD_GETIMAGENAME 6
#define MONOCMD_ENUMCLASSESINIMAGE 7
#define MONOCMD_ENUMFIELDSINCLASS 8
#define MONOCMD_ENUMMETHODSINCLASS 9
#define MONOCMD_COMPILEMETHOD 10

#define MONOCMD_GETMETHODHEADER 11
#define MONOCMD_GETMETHODHEADER_CODE 12
#define MONOCMD_LOOKUPRVA 13
#define MONOCMD_GETJITINFO 14
#define MONOCMD_FINDCLASS 15
#define MONOCMD_FINDMETHOD 16
#define MONOCMD_GETMETHODNAME 17
#define MONOCMD_GETMETHODCLASS 18
#define MONOCMD_GETCLASSNAME 19
#define MONOCMD_GETCLASSNAMESPACE 20
#define MONOCMD_FREEMETHOD 21
#define MONOCMD_TERMINATE 22
#define MONOCMD_DISASSEMBLE 23
#define MONOCMD_GETMETHODSIGNATURE 24
#define MONOCMD_GETPARENTCLASS 25
#define MONOCMD_GETSTATICFIELDADDRESSFROMCLASS 26
#define MONOCMD_GETFIELDCLASS 27
#define MONOCMD_GETARRAYELEMENTCLASS 28
#define MONOCMD_FINDMETHODBYDESC 29
#define MONOCMD_INVOKEMETHOD 30
#define MONOCMD_LOADASSEMBLY 31
#define MONOCMD_GETFULLTYPENAME 32
#define MONOCMD_OBJECT_NEW 33
#define MONOCMD_OBJECT_INIT 34
#define MONOCMD_GETVTABLEFROMCLASS 35
#define MONOCMD_GETMETHODPARAMETERS 36
#define MONOCMD_ISCLASSGENERIC 37
#define MONOCMD_ISIL2CPP 38
#define MONOCMD_FILLOPTIONALFUNCTIONLIST 39
#define MONOCMD_GETSTATICFIELDVALUE 40
#define MONOCMD_SETSTATICFIELDVALUE 41
#define MONOCMD_GETCLASSIMAGE 42
#define MONOCMD_FREE 43
#define MONOCMD_GETIMAGEFILENAME 44
#define MONOCMD_GETCLASSNESTINGTYPE 45
//#define MONOCMD_LIMITEDCONNECTION 46
#define MONOCMD_GETMONODATACOLLECTORVERSION 47
#define MONOCMD_NEWSTRING 48
#define MONOCMD_ENUMIMAGES 49
#define MONOCMD_ENUMCLASSESINIMAGEEX 50
#define MONOCMD_ISCLASSENUM 51
#define MONOCMD_ISCLASSVALUETYPE 52
#define MONOCMD_ISCLASSISSUBCLASSOF 53
#define MONOCMD_ARRAYELEMENTSIZE 54
#define MONOCMD_GETCLASSTYPE 55
#define MONOCMD_GETCLASSOFTYPE 56
#define MONOCMD_GETTYPEOFMONOTYPE 57
#define MONOCMD_GETREFLECTIONTYPEOFCLASSTYPE 58
#define MONOCMD_GETREFLECTIONMETHODOFMONOMETHOD 59
#define MONOCMD_MONOOBJECTUNBOX 60
#define MONOCMD_MONOARRAYNEW 61
#define MONOCMD_ENUMINTERFACESOFCLASS 62
#define MONOCMD_GETMETHODFULLNAME 63
#define MONOCMD_TYPEISBYREF 64
#define MONOCMD_GETPTRTYPECLASS 65
#define MONOCMD_GETFIELDTYPE 66
#define MONOCMD_GETTYPEPTRTYPE 67
#define MONOCMD_GETCLASSNESTEDTYPES 68

#define MONOCMD_COLLECTGARBAGE 69
#define MONOCMD_GETMETHODFLAGS 70

#define MONOCMD_SETMONOLIB 71
#define MONOCMD_ENUMMETHODSINCLASSES 72


typedef struct {} MonoType;
typedef struct {} MonoObject;
typedef struct {} MonoMethodSignature;
typedef void * gpointer;

typedef void (__cdecl *MonoDomainFunc) (void *domain, void *user_data);
typedef void (__cdecl *GFunc)          (void *data, void *user_data);

typedef void (__cdecl *G_FREE)(void *ptr);

typedef void* (__cdecl *MONO_GET_ROOT_DOMAIN)(void);
typedef void* (__cdecl *MONO_THREAD_ATTACH)(void *domain);
typedef void (__cdecl *MONO_THREAD_DETACH)(void *monothread);
typedef void (__cdecl *MONO_THREAD_CLEANUP)(void);
typedef void* (__cdecl *MONO_OBJECT_GET_CLASS)(void *object);

typedef void (__cdecl *MONO_DOMAIN_FOREACH)(MonoDomainFunc func, void *user_data);

typedef int (__cdecl *MONO_DOMAIN_SET)(void *domain, BOOL force);
typedef void* (__cdecl *MONO_DOMAIN_GET)();
typedef int (__cdecl *MONO_ASSEMBLY_FOREACH)(GFunc func, void *user_data);
typedef void* (__cdecl *MONO_ASSEMBLY_GET_IMAGE)(void *assembly);
typedef void* (__cdecl *MONO_ASSEMBLY_OPEN)(void *fname, int *status);
typedef void* (__cdecl *MONO_IMAGE_GET_ASSEMBLY)(void *image);
typedef char* (__cdecl *MONO_IMAGE_GET_NAME)(void *image);
typedef void* (__cdecl *MONO_IMAGE_OPEN)(const char *fname, int *status);
typedef char* (__cdecl *MONO_IMAGE_GET_FILENAME)(void *image);


typedef void* (__cdecl *MONO_IMAGE_GET_TABLE_INFO)(void *image, int table_id);
typedef int (__cdecl *MONO_TABLE_INFO_GET_ROWS)(void *tableinfo);
typedef int (__cdecl *MONO_METADATA_DECODE_ROW_COL)(void *tableinfo, int idx, unsigned int col);
typedef char* (__cdecl *MONO_METADATA_STRING_HEAP)(void *image, UINT32 index);

typedef void* (__cdecl *MONO_CLASS_FROM_NAME_CASE)(void *image, char *name_space, char *name);
typedef void* (__cdecl *MONO_CLASS_FROM_NAME)(void *image, char *name_space, char *name);
typedef char* (__cdecl *MONO_CLASS_GET_NAME)(void *klass);
typedef char* (__cdecl *MONO_CLASS_GET_NAMESPACE)(void *klass);
typedef void* (__cdecl *MONO_CLASS_GET)(void *image, UINT32 tokenindex);
typedef void* (__cdecl *MONO_CLASS_FROM_TYPEREF)(void *image, UINT32 type_token);
typedef char* (__cdecl *MONO_CLASS_NAME_FROM_TOKEN)(void *image, UINT32 token);


typedef void* (__cdecl *MONO_CLASS_GET_METHODS)(void *klass, void *iter);
typedef void* (__cdecl *MONO_CLASS_GET_METHOD_FROM_NAME)(void *klass, char *methodname, int paramcount);
typedef void* (__cdecl *MONO_CLASS_GET_FIELDS)(void *klass, void *iter);
typedef void* (__cdecl *MONO_CLASS_GET_INTERFACES)(void *klass, void *iter);
typedef void* (__cdecl *MONO_CLASS_GET_PARENT)(void *klass);
typedef void* (__cdecl *MONO_CLASS_GET_IMAGE)(void *klass);
typedef void* (__cdecl *MONO_CLASS_VTABLE)(void *domain, void *klass);
typedef int (__cdecl *MONO_CLASS_INSTANCE_SIZE)(void *klass);
typedef void* (__cdecl *MONO_CLASS_FROM_MONO_TYPE)(void *type);
typedef void* (__cdecl *MONO_CLASS_GET_ELEMENT_CLASS)(void *klass);
typedef int (__cdecl *MONO_CLASS_IS_GENERIC)(void *klass);
typedef bool (__cdecl *MONO_CLASS_IS_ENUM)(void *klass);
typedef bool (__cdecl *MONO_CLASS_IS_VALUETYPE)(void *klass);
typedef bool (__cdecl *MONO_CLASS_IS_SUBCLASS_OF)(void *klass, void* parentKlass, bool check_interface);

typedef int (__cdecl *MONO_CLASS_NUM_FIELDS)(void *klass);
typedef int (__cdecl *MONO_CLASS_NUM_METHODS)(void *klass);

typedef char* (__cdecl *MONO_FIELD_GET_NAME)(void *field);
typedef void* (__cdecl *MONO_FIELD_GET_TYPE)(void *field);
typedef void* (__cdecl *MONO_FIELD_GET_PARENT)(void *field);
typedef int (__cdecl *MONO_FIELD_GET_OFFSET)(void *field);

typedef char* (__cdecl *MONO_TYPE_GET_NAME)(void *type);
typedef void* (__cdecl* MONO_TYPE_GET_CLASS)(void* type);
typedef int (__cdecl *MONO_TYPE_GET_TYPE)(void *type);
typedef int (__cdecl *MONO_TYPE_IS_BYREF)(void *monotype);
typedef void* (__cdecl *MONO_TYPE_GET_OBJECT)(void *domain, void *type);
typedef void* (__cdecl *IL2CPP_TYPE_GET_OBJECT)(void *type);
typedef void* (__cdecl *MONO_METHOD_GET_OBJECT)(void *domain, void *method, void* klass);
typedef void* (__cdecl *IL2CPP_METHOD_GET_OBJECT)(void* method, void* klass);
typedef void* (__cdecl* MONO_PTR_GET_CLASS)(void* monotype);
typedef void* (__cdecl* MONO_TYPE_GET_PTR_TYPE)(void* ptrmonotype);


typedef char* (__cdecl *MONO_TYPE_GET_NAME_FULL)(void *type, int format);
typedef bool(__cdecl* MONO_TYPE_IS_STRUCT)(void* type);

typedef int (__cdecl *MONO_FIELD_GET_FLAGS)(void *type);
typedef void* (__cdecl * MONO_FIELD_GET_VALUE_OBJECT)(void *domain, void* field, void* object);

typedef int (__cdecl *MONO_FIELD_GET_FLAGS)(void *type);
typedef void* (__cdecl * MONO_FIELD_GET_VALUE_OBJECT)(void *domain, void* field, void* object);


typedef char* (__cdecl *MONO_METHOD_GET_NAME)(void *method);
typedef char* (__cdecl *MONO_METHOD_GET_FULL_NAME)(void *method);
typedef void* (__cdecl *MONO_COMPILE_METHOD)(void *method);
typedef void (__cdecl *MONO_FREE_METHOD)(void *method);

typedef void* (__cdecl *MONO_JIT_INFO_TABLE_FIND)(void *domain, void *addr);

typedef void* (__cdecl *MONO_JIT_INFO_GET_METHOD)(void *jitinfo);
typedef void* (__cdecl *MONO_JIT_INFO_GET_CODE_START)(void *jitinfo);
typedef int (__cdecl *MONO_JIT_INFO_GET_CODE_SIZE)(void *jitinfo);

typedef int (__cdecl *MONO_JIT_EXEC)(void *domain, void *assembly, int argc, char *argv[]);
	

typedef uint32_t (__cdecl *MONO_METHOD_GET_FLAGS)(void *method, uint32_t *iflags);
typedef void* (__cdecl *MONO_METHOD_GET_HEADER)(void *method);
typedef void* (__cdecl *MONO_METHOD_GET_CLASS)(void *method);
typedef void* (__cdecl *MONO_METHOD_SIG)(void *method);
typedef void* (__cdecl *MONO_METHOD_GET_PARAM_NAMES)(void *method, const char **names);

typedef void* (__cdecl *MONO_METHOD_HEADER_GET_CODE)(void *methodheader, UINT32 *code_size, UINT32 *max_stack);
typedef char* (__cdecl *MONO_DISASM_CODE)(void *dishelper, void *method, void *ip, void *end);

typedef char* (__cdecl *MONO_SIGNATURE_GET_DESC)(void *signature, int include_namespace);
typedef MonoType* (__cdecl *MONO_SIGNATURE_GET_PARAMS)(MonoMethodSignature *sig, gpointer *iter);
typedef int (__cdecl *MONO_SIGNATURE_GET_PARAM_COUNT)(void *signature);
typedef MonoType* (__cdecl *MONO_SIGNATURE_GET_RETURN_TYPE)(void *signature);


typedef void* (__cdecl *MONO_IMAGE_RVA_MAP)(void *image, UINT32 addr);
typedef void* (__cdecl *MONO_VTABLE_GET_STATIC_FIELD_DATA)(void *vtable);


typedef void* (__cdecl *MONO_METHOD_DESC_NEW)(const char *name, int include_namespace);
typedef void* (__cdecl *MONO_METHOD_DESC_FROM_METHOD)(void *method);
typedef void  (__cdecl *MONO_METHOD_DESC_FREE)(void *desc);

typedef void* (__cdecl *MONO_ASSEMBLY_NAME_NEW)(const char *name);
typedef void* (__cdecl *MONO_ASSEMBLY_LOADED)(void *aname);
typedef void* (__cdecl *MONO_IMAGE_LOADED)(void *aname);

typedef void* (__cdecl *MONO_STRING_NEW)(void *domain, const char *text);
typedef char* (__cdecl *MONO_STRING_TO_UTF8)(void*);
typedef void* (__cdecl *MONO_ARRAY_NEW)(void *domain, void *eclass, uintptr_t n);
typedef void* (__cdecl *IL2CPP_ARRAY_NEW)(void *eclass, uintptr_t n);
typedef int (__cdecl *MONO_ARRAY_ELEMENT_SIZE)(void * klass);
typedef int(__cdecl *MONO_CLASS_GET_RANK)(void * klass);
typedef void* (__cdecl *MONO_OBJECT_TO_STRING)(void *object, void **exc);
typedef void* (__cdecl *MONO_OBJECT_NEW)(void *domain, void *klass);


typedef void  (__cdecl *MONO_FREE)(void*);

typedef void* (__cdecl *MONO_METHOD_DESC_SEARCH_IN_IMAGE)(void *desc, void *image);
typedef void* (__cdecl *MONO_RUNTIME_INVOKE)(void *method, void *obj, void **params, MonoObject **exc);
typedef void* (__cdecl *MONO_RUNTIME_INVOKE_ARRAY)(void *method, void *obj, void *params, void **exc);
typedef void* (__cdecl *MONO_RUNTIME_OBJECT_INIT)(void *object);

typedef void* (__cdecl *MONO_FIELD_STATIC_GET_VALUE)(void *vtable, void* field, void* output);
typedef void* (__cdecl *MONO_FIELD_STATIC_SET_VALUE)(void *vtable, void* field, void* input);

typedef void* (__cdecl *IL2CPP_FIELD_STATIC_GET_VALUE)(void* field, void* output);
typedef void* (__cdecl *IL2CPP_FIELD_STATIC_SET_VALUE)(void* field, void* input);

typedef void* (__cdecl *IL2CPP_CLASS_FROM_IL2CPP_TYPE)(void* field);



typedef void* (__cdecl *MONO_VALUE_BOX)(void *domain, void *klass, void* val);
typedef void* (__cdecl *MONO_OBJECT_UNBOX)(void *obj);
typedef void* (__cdecl *MONO_OBJECT_ISINST)(void *obj, void* kls);
typedef void* (__cdecl *MONO_GET_ENUM_CLASS)(void);
typedef void* (__cdecl *MONO_CLASS_GET_TYPE)(void *klass);
typedef void* (__cdecl *MONO_CLASS_GET_NESTING_TYPE)(void *klass);

typedef void* (__cdecl *MONO_CLASS_GET_NESTED_TYPES)(void *klass, void* iter);


typedef int (__cdecl *MONO_RUNTIME_IS_SHUTTING_DOWN)(void);



//il2cpp:
typedef UINT_PTR* (__cdecl *IL2CPP_DOMAIN_GET_ASSEMBLIES)(void * domain, SIZE_T *size);

typedef int(__cdecl *IL2CPP_IMAGE_GET_CLASS_COUNT)(void* image);
typedef void*(__cdecl *IL2CPP_IMAGE_GET_CLASS)(void *image, int index);

typedef char*(__cdecl *IL2CPP_TYPE_GET_NAME)(void* ptype);
typedef char*(__cdecl *IL2CPP_TYPE_GET_ASSEMBLY_QUALIFIED_NAME)(void* ptype);

typedef int(__cdecl *IL2CPP_METHOD_GET_PARAM_COUNT)(void* method);
typedef char*(__cdecl *IL2CPP_METHOD_GET_PARAM_NAME)(void *method, int index);
typedef void*(__cdecl *IL2CPP_METHOD_GET_PARAM)(void *method, int index);
typedef void*(__cdecl *IL2CPP_METHOD_GET_RETURN_TYPE)(void *method);
typedef void*(__cdecl *IL2CPP_CLASS_FROM_TYPE)(void *type);
typedef wchar_t*(__cdecl *IL2CPP_STRING_CHARS)(void *stringobject);

typedef void*(__cdecl *IL2CPP_CLASS_GET_STATIC_FIELD_DATA)(void *klass);


void InitMono();



class CWorker : Pipe //each client is a seperate thread
{
private:
	BOOL attached;

	

	void *mono_selfthread;
	void ConnectThreadToMonoRuntime();
  void DetachThreadFromMonoRuntime();

	void SetMonoLib();

	void Object_GetClass();
	void EnumDomains();
	void SetCurrentDomain();
	void EnumAssemblies();
	void GetImageFromAssembly();
	void GetImageName();
	void GetImageFileName();
	void EnumImages();
	void EnumClassesInImage();
	void EnumClassesInImageEx();
	void EnumFieldsInClass();
	void EnumImplementedInterfacesOfClass();
  void EnumMethodsInClasses();
	void EnumMethodsInClass();
	void CompileMethod();
	void GetMethodHeader();
	void GetILCode();
	void RvaMap();
	void GetJitInfo();
	void FindClass();
	void FindMethod();
	void GetMethodName();
	void GetMethodFullName();
	void GetMethodClass();
	void GetMethodFlags();
	void GetKlassName();
	void GetClassNamespace();
	void FreeMethod();
	void FreeObject();
	void DisassembleMethod();
	void GetMethodSignature();
	void GetMethodParameters();
	void GetParentClass();
	void GetClassNestedTypes();
	void GetClassNestingType();
	void GetClassImage();
	void GetClassType();
	void GetClassOfType();
	void GetTypeOfMonoType();
	void GetReflectionTypeOfClassType();
	void GetReflectionMethodOfMethod();
	void UnBoxMonoObject();
	void GetVTableFromClass();
	void GetStaticFieldAddressFromClass();
	void GetFieldClass();
	void GetFieldType();
	void GetArrayElementClass();
	void FindMethodByDesc();
	void InvokeMethod();
	void LoadAssemblyFromFile();
	void GetFullTypeName();
	std::string GetFullTypeNameStr(void* klass, char isKlass, int nameformat);
	void Object_New();
	void Object_Init();
	void IsGenericClass();
	void IsEnumClass();
	void IsValueTypeClass();
	void IsSubClassOf();
	void IsTypeByReference();
	void GetArrayElementSize();
	void NewCSArray();
	void IsIL2CPP();
	void FillOptionalFunctionList(); //mainly for unixbased systems
	void GetStaticFieldValue();
	void SetStaticFieldValue();
	void GetMonoDataCollectorVersion();
	void NewString();

	void GetClassFromPointer();
	void GetTypeFromPointerType();

	void CommandLoop(void);

	char* ReadString(void);
	void WriteString(const char*);
	void WriteString1(const char*);
	void FreeString(char*);



public:
	jmp_buf onError;
	void workerThreadEntry();
#ifdef _WINDOWS

	DWORD threadid;
#else
	uint64_t threadid;
#endif
	bool ExpectingAccessViolations;


    bool finished; //set to true when the Commandloop has exited
	void SetPipeHandle(HANDLE newPipeHandle);
    void Start();    
	CWorker(HANDLE p);
	~CWorker();
};

class CPipeServer
{
private:
    #ifdef _WINDOWS
	wchar_t datapipename[256];
	wchar_t eventpipename[256];
    #else
    char* datapipename[256];
    char* eventpipename[256];
    #endif

	BOOL limitedConnection;
	BOOL UWPMode;
  HANDLE pipehandle;
     	

public:
	void CreatePipeAndSpawnWorkers(void);
  void CheckForShutdown(void);
	CPipeServer(void);
	~CPipeServer(void);

};
