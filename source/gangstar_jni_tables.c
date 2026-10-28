#include <falso_jni/FalsoJNI.h>
#include <falso_jni/FalsoJNI_Impl.h>

/* Gangstar installs its class/signature-aware bridge before JNI_OnLoad.
 * No static game callbacks or Java fields are registered with FalsoJNI. */
NameToMethodID nameToMethodId[] = {};
NameToFieldID nameToFieldId[] = {};
MethodsBoolean methodsBoolean[] = {};
MethodsByte methodsByte[] = {};
MethodsChar methodsChar[] = {};
MethodsDouble methodsDouble[] = {};
MethodsFloat methodsFloat[] = {};
MethodsInt methodsInt[] = {};
MethodsLong methodsLong[] = {};
MethodsObject methodsObject[] = {};
MethodsShort methodsShort[] = {};
MethodsVoid methodsVoid[] = {};
FieldsBoolean fieldsBoolean[] = {};
FieldsByte fieldsByte[] = {};
FieldsChar fieldsChar[] = {};
FieldsDouble fieldsDouble[] = {};
FieldsFloat fieldsFloat[] = {};
FieldsInt fieldsInt[] = {};
FieldsLong fieldsLong[] = {};
FieldsObject fieldsObject[] = {};
FieldsShort fieldsShort[] = {};
__FALSOJNI_IMPL_CONTAINER_SIZES
