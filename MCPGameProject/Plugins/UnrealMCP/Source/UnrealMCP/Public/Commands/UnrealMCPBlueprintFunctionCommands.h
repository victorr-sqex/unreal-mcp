#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Blueprint Function-related MCP commands
 * Handles function discovery, analysis, manipulation, and interface operations
 */
class UNREALMCP_API FUnrealMCPBlueprintFunctionCommands
{
public:
    FUnrealMCPBlueprintFunctionCommands();

    // Main command dispatcher
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Function discovery and inspection
    TSharedPtr<FJsonObject> HandleFindFunctionsByName(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetFunctionSignature(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetFunctionGraph(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListAllFunctions(const TSharedPtr<FJsonObject>& Params);

    // Interface operations
    TSharedPtr<FJsonObject> HandleFindInterfaceImplementers(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetInterfaceFunctions(const TSharedPtr<FJsonObject>& Params);

    // Function manipulation
    TSharedPtr<FJsonObject> HandleRenameFunction(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRemoveInterfaceFunction(const TSharedPtr<FJsonObject>& Params);

    // Custom properties / details panel
    TSharedPtr<FJsonObject> HandleGetBlueprintCustomProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetBlueprintCustomProperties(const TSharedPtr<FJsonObject>& Params);

    // Helper methods
    TSharedPtr<FJsonObject> BuildFunctionMetadata(UFunction* Function, const FString& FunctionName);
    TSharedPtr<FJsonObject> BuildGraphAnalysis(UBlueprint* Blueprint, UFunction* Function);
};
