#include "Commands/UnrealMCPBlueprintFunctionCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_FunctionCall.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Event.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/Blueprint.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealMCPFunctions, Log, All);

FUnrealMCPBlueprintFunctionCommands::FUnrealMCPBlueprintFunctionCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintFunctionCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("find_functions_by_name"))
    {
        return HandleFindFunctionsByName(Params);
    }
    else if (CommandType == TEXT("get_function_signature"))
    {
        return HandleGetFunctionSignature(Params);
    }
    else if (CommandType == TEXT("get_function_graph"))
    {
        return HandleGetFunctionGraph(Params);
    }
    else if (CommandType == TEXT("list_all_functions"))
    {
        return HandleListAllFunctions(Params);
    }
    else if (CommandType == TEXT("find_interface_implementers"))
    {
        return HandleFindInterfaceImplementers(Params);
    }
    else if (CommandType == TEXT("get_interface_functions"))
    {
        return HandleGetInterfaceFunctions(Params);
    }
    else if (CommandType == TEXT("rename_function"))
    {
        return HandleRenameFunction(Params);
    }
    else if (CommandType == TEXT("remove_interface_function"))
    {
        return HandleRemoveInterfaceFunction(Params);
    }
    else if (CommandType == TEXT("get_blueprint_custom_properties"))
    {
        return HandleGetBlueprintCustomProperties(Params);
    }
    else if (CommandType == TEXT("set_blueprint_custom_properties"))
    {
        return HandleSetBlueprintCustomProperties(Params);
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown blueprint function command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintFunctionCommands::HandleFindFunctionsByName(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString SearchName;
    if (!Params->TryGetStringField(TEXT("search_name"), SearchName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'search_name' parameter"));
    }

    // Optional parameters for exact matching
    bool bExactMatch = false;
    if (Params->HasField(TEXT("exact_match")))
    {
        bExactMatch = Params->GetBoolField(TEXT("exact_match"));
    }

    bool bCaseSensitive = false;
    if (Params->HasField(TEXT("case_sensitive")))
    {
        bCaseSensitive = Params->GetBoolField(TEXT("case_sensitive"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    TArray<TSharedPtr<FJsonValue>> FoundFunctions;
    
    // Search through the blueprint's functions
    for (const FName& FuncName : Blueprint->FunctionNames)
    {
        FString FuncNameStr = FuncName.ToString();
        bool bMatches = false;

        if (bExactMatch)
        {
            if (bCaseSensitive)
            {
                bMatches = (FuncNameStr == SearchName);
            }
            else
            {
                bMatches = FuncNameStr.Equals(SearchName, ESearchCase::IgnoreCase);
            }
        }
        else
        {
            // Partial match
            if (bCaseSensitive)
            {
                bMatches = FuncNameStr.Contains(SearchName);
            }
            else
            {
                bMatches = FuncNameStr.Contains(SearchName, ESearchCase::IgnoreCase);
            }
        }

        if (bMatches)
        {
            UFunction* Func = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->FindFunctionByName(FuncName) : nullptr;
            if (Func)
            {
                TSharedPtr<FJsonObject> FuncInfo = BuildFunctionMetadata(Func, FuncNameStr);
                FoundFunctions.Add(MakeShared<FJsonValueObject>(FuncInfo));
            }
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("functions"), FoundFunctions);
    ResultObj->SetNumberField(TEXT("count"), FoundFunctions.Num());
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintFunctionCommands::HandleGetFunctionSignature(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString FunctionName;
    if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UFunction* Func = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->FindFunctionByName(*FunctionName) : nullptr;
    if (!Func)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Function not found: %s"), *FunctionName));
    }

    return BuildFunctionMetadata(Func, FunctionName);
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintFunctionCommands::BuildFunctionMetadata(UFunction* Function, const FString& FunctionName)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), FunctionName);
    Result->SetStringField(TEXT("return_type"), Function->GetReturnProperty() ? Function->GetReturnProperty()->GetCPPType() : TEXT("void"));

    // Get function flags
    Result->SetBoolField(TEXT("is_pure"), Function->HasMetaData(TEXT("BlueprintPure")));
    Result->SetBoolField(TEXT("is_callable"), Function->HasAnyFunctionFlags(FUNC_BlueprintCallable));
    Result->SetBoolField(TEXT("is_event"), Function->HasAnyFunctionFlags(FUNC_Event));
    Result->SetBoolField(TEXT("is_const"), Function->HasAnyFunctionFlags(FUNC_Const));

    // Build parameters list
    TArray<TSharedPtr<FJsonValue>> ParametersArray;
    for (TFieldIterator<FProperty> PropIt(Function); PropIt; ++PropIt)
    {
        FProperty* Prop = *PropIt;
        if (Prop->HasAnyPropertyFlags(CPF_Parm) && !Prop->HasAnyPropertyFlags(CPF_ReturnParm))
        {
            TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
            ParamObj->SetStringField(TEXT("name"), Prop->GetName());
            ParamObj->SetStringField(TEXT("type"), Prop->GetCPPType());
            ParamObj->SetBoolField(TEXT("is_ref"), Prop->HasAnyPropertyFlags(CPF_OutParm));
            ParametersArray.Add(MakeShared<FJsonValueObject>(ParamObj));
        }
    }
    Result->SetArrayField(TEXT("parameters"), ParametersArray);

    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintFunctionCommands::HandleGetFunctionGraph(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString FunctionName;
    if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Find the function graph
    UEdGraph* FuncGraph = nullptr;
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (Graph && Graph->GetName() == FunctionName)
        {
            FuncGraph = Graph;
            break;
        }
    }

    if (!FuncGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Function graph not found: %s"), *FunctionName));
    }

    TSharedPtr<FJsonObject> GraphAnalysis = BuildGraphAnalysis(Blueprint, nullptr);
    GraphAnalysis->SetStringField(TEXT("function_name"), FunctionName);

    // Analyze nodes in the function graph
    TArray<TSharedPtr<FJsonValue>> NodesArray;
    for (UEdGraphNode* Node : FuncGraph->Nodes)
    {
        TSharedPtr<FJsonObject> NodeInfo = MakeShared<FJsonObject>();
        NodeInfo->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
        NodeInfo->SetStringField(TEXT("node_type"), Node->GetClass()->GetName());
        NodeInfo->SetStringField(TEXT("node_title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

        // Count pins by direction
        int32 InputCount = 0, OutputCount = 0;
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin->Direction == EGPD_Input)
                InputCount++;
            else
                OutputCount++;
        }
        NodeInfo->SetNumberField(TEXT("input_pins"), InputCount);
        NodeInfo->SetNumberField(TEXT("output_pins"), OutputCount);

        NodesArray.Add(MakeShared<FJsonValueObject>(NodeInfo));
    }
    GraphAnalysis->SetArrayField(TEXT("nodes"), NodesArray);

    return GraphAnalysis;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintFunctionCommands::BuildGraphAnalysis(UBlueprint* Blueprint, UFunction* Function)
{
    TSharedPtr<FJsonObject> Analysis = MakeShared<FJsonObject>();
    Analysis->SetStringField(TEXT("blueprint"), Blueprint->GetName());
    return Analysis;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintFunctionCommands::HandleListAllFunctions(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    TArray<TSharedPtr<FJsonValue>> AllFunctions;
    
    for (const FName& FuncName : Blueprint->FunctionNames)
    {
        UFunction* Func = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->FindFunctionByName(FuncName) : nullptr;
        if (Func)
        {
            TSharedPtr<FJsonObject> FuncInfo = BuildFunctionMetadata(Func, FuncName.ToString());
            AllFunctions.Add(MakeShared<FJsonValueObject>(FuncInfo));
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("functions"), AllFunctions);
    ResultObj->SetNumberField(TEXT("count"), AllFunctions.Num());
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintFunctionCommands::HandleFindInterfaceImplementers(const TSharedPtr<FJsonObject>& Params)
{
    FString InterfaceName;
    if (!Params->TryGetStringField(TEXT("interface_name"), InterfaceName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'interface_name' parameter"));
    }

    // Try to find the interface blueprint
    UBlueprint* InterfaceBlueprint = FUnrealMCPCommonUtils::FindBlueprint(InterfaceName);
    if (!InterfaceBlueprint || !FKismetEditorUtilities::IsActorBased(InterfaceBlueprint->GeneratedClass))
    {
        // Try searching by class
        UClass* InterfaceClass = FindObject<UClass>(ANY_PACKAGE, *InterfaceName);
        if (!InterfaceClass || !InterfaceClass->IsChildOf<UInterface>())
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Interface not found: %s"), *InterfaceName));
        }
    }

    TArray<TSharedPtr<FJsonValue>> ImplementersArray;
    
    // Search all blueprints in memory for ones that implement this interface
    for (TObjectIterator<UBlueprint> BlueprintIt; BlueprintIt; ++BlueprintIt)
    {
        UBlueprint* BP = *BlueprintIt;
        if (BP && BP->GeneratedClass)
        {
            // Check if this blueprint's class implements the interface
            for (const FImplementedInterface& Impl : BP->GeneratedClass->Interfaces)
            {
                if (Impl.Class && Impl.Class->GetName().Contains(InterfaceName, ESearchCase::IgnoreCase))
                {
                    TSharedPtr<FJsonObject> ImplementerInfo = MakeShared<FJsonObject>();
                    ImplementerInfo->SetStringField(TEXT("blueprint_name"), BP->GetName());
                    ImplementerInfo->SetStringField(TEXT("class_name"), BP->GeneratedClass->GetName());
                    ImplementersArray.Add(MakeShared<FJsonValueObject>(ImplementerInfo));
                    break;
                }
            }
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("implementers"), ImplementersArray);
    ResultObj->SetNumberField(TEXT("count"), ImplementersArray.Num());
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintFunctionCommands::HandleGetInterfaceFunctions(const TSharedPtr<FJsonObject>& Params)
{
    FString InterfaceName;
    if (!Params->TryGetStringField(TEXT("interface_name"), InterfaceName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'interface_name' parameter"));
    }

    UBlueprint* InterfaceBlueprint = FUnrealMCPCommonUtils::FindBlueprint(InterfaceName);
    if (!InterfaceBlueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Interface blueprint not found: %s"), *InterfaceName));
    }

    TArray<TSharedPtr<FJsonValue>> FunctionsArray;

    for (const FName& FuncName : InterfaceBlueprint->FunctionNames)
    {
        UFunction* Func = InterfaceBlueprint->GeneratedClass ? InterfaceBlueprint->GeneratedClass->FindFunctionByName(FuncName) : nullptr;
        if (Func)
        {
            TSharedPtr<FJsonObject> FuncInfo = BuildFunctionMetadata(Func, FuncName.ToString());
            FunctionsArray.Add(MakeShared<FJsonValueObject>(FuncInfo));
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("interface_name"), InterfaceName);
    ResultObj->SetArrayField(TEXT("functions"), FunctionsArray);
    ResultObj->SetNumberField(TEXT("count"), FunctionsArray.Num());
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintFunctionCommands::HandleRenameFunction(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString OldName;
    if (!Params->TryGetStringField(TEXT("old_name"), OldName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'old_name' parameter"));
    }

    FString NewName;
    if (!Params->TryGetStringField(TEXT("new_name"), NewName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'new_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Find the function to rename
    UEdGraph* FuncGraph = nullptr;
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (Graph && Graph->GetName() == OldName)
        {
            FuncGraph = Graph;
            break;
        }
    }

    if (!FuncGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Function not found: %s"), *OldName));
    }

    // Rename the function using Kismet editor utilities
    FBlueprintEditorUtils::RenameGraph(FuncGraph, NewName);
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("old_name"), OldName);
    ResultObj->SetStringField(TEXT("new_name"), NewName);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintFunctionCommands::HandleRemoveInterfaceFunction(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString FunctionName;
    if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Find and remove the function graph
    UEdGraph* FuncGraph = nullptr;
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (Graph && Graph->GetName() == FunctionName)
        {
            FuncGraph = Graph;
            break;
        }
    }

    if (!FuncGraph)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Function not found: %s"), *FunctionName));
    }

    // Remove from function graphs and mark modified
    Blueprint->FunctionGraphs.Remove(FuncGraph);
    FBlueprintEditorUtils::RemoveFunctionResult(Blueprint, FName(*FunctionName));
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("function_name"), FunctionName);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintFunctionCommands::HandleGetBlueprintCustomProperties(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    TSharedPtr<FJsonObject> CustomPropsObj = MakeShared<FJsonObject>();

    // Get class default object to read custom properties
    UObject* CDO = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;
    if (CDO)
    {
        for (TFieldIterator<FProperty> PropIt(Blueprint->GeneratedClass); PropIt; ++PropIt)
        {
            FProperty* Prop = *PropIt;
            if (!Prop->HasAnyPropertyFlags(CPF_Edit | CPF_EditConst))
                continue;

            // Read property value as string
            FString PropValue;
            Prop->ExportText_InContainer(0, PropValue, CDO, CDO, CDO, 0);
            CustomPropsObj->SetStringField(Prop->GetName(), PropValue);
        }
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
    ResultObj->SetObjectField(TEXT("properties"), CustomPropsObj);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPBlueprintFunctionCommands::HandleSetBlueprintCustomProperties(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    const TSharedPtr<FJsonObject>* PropsObj = nullptr;
    if (!Params->TryGetObjectField(TEXT("properties"), PropsObj))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'properties' parameter"));
    }

    UBlueprint* Blueprint = FUnrealMCPCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UObject* CDO = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;
    if (!CDO)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get class default object"));
    }

    TSharedPtr<FJsonObject> SetPropsObj = MakeShared<FJsonObject>();

    for (const auto& Pair : (*PropsObj)->Values)
    {
        FString PropertyName = Pair.Key;
        FProperty* Prop = Blueprint->GeneratedClass->FindPropertyByName(*PropertyName);
        
        if (Prop && Pair.Value.IsValid())
        {
            FString PropValue = Pair.Value->AsString();
            Prop->ImportText(*PropValue, Prop->ContainerPtrToValuePtr<void>(CDO), 0, CDO);
            SetPropsObj->SetBoolField(PropertyName, true);
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
    ResultObj->SetObjectField(TEXT("set_properties"), SetPropsObj);
    return ResultObj;
}
