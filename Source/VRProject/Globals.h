#pragma once
//#include "CoreMinimal.h"
//#include "Object.h"
//#include "Globals.generated.h"

//===============================
// Remove debug code from builds.
//===============================

#define DEVELOPMENT 1

//===============================
// Misc
//===============================

#define MAX_WIDGET_POOL_SIZE 30

// Enable temporal AA anti ghosting feature.
#define AA_DYNAMIC_ANTIGHOST 1

//==============================
// Collisions Object types
//===============================

#define ECC_Hand ECC_GameTraceChannel1
#define ECC_Teleport ECC_GameTraceChannel2
#define ECC_UI ECC_GameTraceChannel3
#define ECC_Walkable ECC_GameTraceChannel4
#define ECC_Interactable ECC_GameTraceChannel5

//=======================
// Macros
//=======================

// Checks condition, if true, will log STRING and return.
#define CHECK_RETURN(CategoryName, condition, logString, ...) if (condition) {UE_LOG(CategoryName, Error, TEXT(logString), ##__VA_ARGS__); return;};
// Checks condition, if true, will log STRING and return.
#define CHECK_RETURN_FALSE(CategoryName, condition, logString, ...) if (condition) {UE_LOG(CategoryName, Error, TEXT(logString), ##__VA_ARGS__); return false;};
// Checks condition, if true, will log STRING and return.
#define CHECK_RETURN_WARNING(CategoryName, condition, logString, ...) if (condition) {UE_LOG(CategoryName, Warning, TEXT(logString), ##__VA_ARGS__); return;};
// Checks condition, if true, will log STRING and return.
#define CHECK_OBJECT_RETURN_WARNING(CategoryName, condition, object, logString, ...) if (condition) {UE_LOG(CategoryName, Warning, TEXT(logString), ##__VA_ARGS__); return object;};
// Checks condition, if true, will log STRING and return object.
#define CHECK_OBJECT_RETURN(CategoryName, condition, object, logString, ...) if (condition) {UE_LOG(CategoryName, Error, TEXT(logString), ##__VA_ARGS__); return object;};
// Checks condition, if true, will log STRING and return nullptr.
#define CHECK_RETURN_NULL(CategoryName, condition, logString, ...) if (condition) {UE_LOG(CategoryName, Error, TEXT(logString), ##__VA_ARGS__); return nullptr;};
// Checks condition, if true, will log STRING and will exit the current loop and run continue command
#define CHECK_CONTINUE(CategoryName, condition, logString, ...) if (condition) {UE_LOG(CategoryName, Error, TEXT(logString), ##__VA_ARGS__); continue;};
// Checks condition, if true, will log STRING and return.
#define RETURN(condition) if (condition) {return;};
// Will print string to the log file if the condition is true
#define CHECK_LOG(CategoryName, verbosity, condition, logString, ...) if (condition) {UE_LOG(CategoryName, verbosity, TEXT(logString), ##__VA_ARGS__);};
// Print log message, shortening macro.
#define PRINT(logString, ...) {UE_LOG(LogTemp, Warning, TEXT(logString), ##__VA_ARGS__);};
// Logging macro for float variables.
#define PRINTF(floatVariable) { UE_LOG(LogTemp, Warning, TEXT("%f"), floatVariable);};
// Logging macro for boolean variables.
#define PRINTB(condition) { UE_LOG(LogTemp, Warning, TEXT("%s"), condition ? TEXT("True") : TEXT("False")); };
// Return a boolean as a String.
#define SBOOL(condition) condition ? TEXT("true") : TEXT("false")
// Return null or valid as text for condition.
#define SNULL(condition) condition ? TEXT("Valid") : TEXT("Nullptr")