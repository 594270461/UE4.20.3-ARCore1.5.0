// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraNodeFunctionCall.h"
#include "UObject/UnrealType.h"
#include "NiagaraGraph.h"
#include "NiagaraScriptSource.h"
#include "NiagaraScript.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "EdGraphSchema_Niagara.h"
#include "Modules/ModuleManager.h"
#include "AssetRegistryModule.h"
#include "NiagaraComponent.h"
#include "NiagaraHlslTranslator.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraNodeParameterMapGet.h"
#include "NiagaraConstants.h"
#include "NiagaraCustomVersion.h"

#define LOCTEXT_NAMESPACE "NiagaraNodeFunctionCall"

void UNiagaraNodeFunctionCall::PostLoad()
{
	Super::PostLoad();

	if (FunctionScript)
	{
		FunctionScript->ConditionalPostLoad();

		// We need to make sure that the variables that could potentially be used in AllocateDefaultPins have been properly
		// loaded. Otherwise, we could be out of date.
		if (FunctionScript->GetSource())
		{
			UNiagaraScriptSource* Source = CastChecked<UNiagaraScriptSource>(FunctionScript->GetSource());
			Source->ConditionalPostLoad();
			UNiagaraGraph* Graph = Source->NodeGraph;
			Graph->ConditionalPostLoad();

			// Fix up autogenerated default values if neccessary.
			const int32 NiagaraCustomVersion = GetLinkerCustomVersion(FNiagaraCustomVersion::GUID);
			if (NiagaraCustomVersion < FNiagaraCustomVersion::EnabledAutogeneratedDefaultValuesForFunctionCallNodes)
			{
				TArray<UEdGraphPin*> InputPins;
				GetInputPins(InputPins);

				TArray<UNiagaraNodeInput*> InputNodes;
				UNiagaraGraph::FFindInputNodeOptions Options;
				Options.bSort = true;
				Options.bFilterDuplicates = true;
				Graph->FindInputNodes(InputNodes, Options);

				for (UEdGraphPin* InputPin : InputPins)
				{
					auto FindInputNodeByName = [&](UNiagaraNodeInput* InputNode) { return InputNode->Input.GetName().ToString() == InputPin->PinName.ToString(); };
					UNiagaraNodeInput** MatchingInputNodePtr = InputNodes.FindByPredicate(FindInputNodeByName);
					if (MatchingInputNodePtr != nullptr)
					{
						UNiagaraNodeInput* MatchingInputNode = *MatchingInputNodePtr;
						SetPinAutoGeneratedDefaultValue(*InputPin, *MatchingInputNode);

						// If the default value wasn't set, update it with the new autogenerated default.
						if (InputPin->DefaultValue.IsEmpty())
						{
							InputPin->DefaultValue = InputPin->AutogeneratedDefaultValue;
						}
					}
				}
			}
		}
	}

	if (FunctionDisplayName.IsEmpty())
	{
		ComputeNodeName();
	}
}

void UNiagaraNodeFunctionCall::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != nullptr)
	{
		ReallocatePins();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);

	MarkNodeRequiresSynchronization(__FUNCTION__, true);
	//GetGraph()->NotifyGraphChanged();
}

void UNiagaraNodeFunctionCall::AllocateDefaultPins()
{
	if (FunctionScriptAssetObjectPath != NAME_None && FunctionScript == nullptr)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		FAssetData ScriptAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FunctionScriptAssetObjectPath);
		if (ScriptAssetData.IsValid())
		{
			FunctionScript = Cast<UNiagaraScript>(ScriptAssetData.GetAsset());
		}
	}

	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());
	UNiagaraGraph* CallerGraph = GetNiagaraGraph();
	if (FunctionScript)
	{
		UNiagaraScriptSource* Source = CastChecked<UNiagaraScriptSource>(FunctionScript->GetSource());
		UNiagaraGraph* Graph = Source->NodeGraph;

		//These pins must be refreshed and kept in the correct order for the function
		TArray<FNiagaraVariable> Inputs;
		TArray<FNiagaraVariable> Outputs;
		Graph->GetParameters(Inputs, Outputs);

		TArray<UNiagaraNodeInput*> InputNodes;
		UNiagaraGraph::FFindInputNodeOptions Options;
		Options.bSort = true;
		Options.bFilterDuplicates = true;
		Graph->FindInputNodes(InputNodes, Options);

		bool bHasAdvancePins = false;
		for (UNiagaraNodeInput* InputNode : InputNodes)
		{
			if (InputNode->IsExposed())
			{
				UEdGraphPin* NewPin = CreatePin(EGPD_Input, Schema->TypeDefinitionToPinType(InputNode->Input.GetType()), InputNode->Input.GetName());
					
				//An inline pin default only makes sense if we are required. 
				//Non exposed or optional inputs will used their own function input nodes defaults when not directly provided by a link.
				//Special class types cannot have an inline default.
				NewPin->bDefaultValueIsIgnored = !(InputNode->IsRequired() && InputNode->Input.GetType().GetClass() == nullptr);

				SetPinAutoGeneratedDefaultValue(*NewPin, *InputNode);
				NewPin->DefaultValue = NewPin->AutogeneratedDefaultValue;

				//TODO: Some visual indication of Auto bound pins.
				//I tried just linking to null but
// 				FNiagaraVariable AutoBoundVar;
// 				ENiagaraInputNodeUsage AutBoundUsage = ENiagaraInputNodeUsage::Undefined;
// 				bool bCanAutoBind = FindAutoBoundInput(InputNode->AutoBindOptions, NewPin, AutoBoundVar, AutBoundUsage);
// 				if (bCanAutoBind)
// 				{
// 
// 				}

				if (InputNode->IsHidden())
				{
					NewPin->bAdvancedView = true;
					bHasAdvancePins = true;
				}
				else
				{
					NewPin->bAdvancedView = false;
				}
			}
		}

		AdvancedPinDisplay = bHasAdvancePins ? ENodeAdvancedPins::Hidden : ENodeAdvancedPins::NoPins;

		for (FNiagaraVariable& Output : Outputs)
		{
			UEdGraphPin* NewPin = CreatePin(EGPD_Output, Schema->TypeDefinitionToPinType(Output.GetType()), Output.GetName());
			NewPin->bDefaultValueIsIgnored = true;
		}

		// Make sure to note that we've synchronized with the external version.
		CachedChangeId = Graph->GetChangeID();
	}
	else
	{
		
		for (FNiagaraVariable& Input : Signature.Inputs)
		{
			UEdGraphPin* NewPin = CreatePin(EGPD_Input, Schema->TypeDefinitionToPinType(Input.GetType()), Input.GetName());
			NewPin->bDefaultValueIsIgnored = false;
		}

		for (FNiagaraVariable& Output : Signature.Outputs)
		{
			UEdGraphPin* NewPin = CreatePin(EGPD_Output, Schema->TypeDefinitionToPinType(Output.GetType()), Output.GetName());
			NewPin->bDefaultValueIsIgnored = true;
		}

		if (AllowDynamicPins())
		{
			CreateAddPin(EGPD_Input);
			CreateAddPin(EGPD_Output);
		}

		// We don't reference an external function, so set an invalid id.
		CachedChangeId = FGuid();
	}

	if (FunctionDisplayName.IsEmpty())
	{
		ComputeNodeName();
	}
}

FText UNiagaraNodeFunctionCall::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FString DetectedName = FunctionScript ? FunctionScript->GetName() : Signature.GetName();
	if (DetectedName.IsEmpty())
	{
		return FText::FromString(TEXT("Missing ( Was\"") + FunctionDisplayName + TEXT("\")"));
	}
	else
	{
		return FText::FromString(FName::NameToDisplayString(FunctionDisplayName, false));
	}
}

FText UNiagaraNodeFunctionCall::GetTooltipText()const
{
	if (FunctionScript != nullptr)
	{
		return FunctionScript->GetDescription();
	}
	else if (Signature.IsValid())
	{
		return Signature.Description;
	} 
	else
	{
		return LOCTEXT("NiagaraFuncCallUnknownSignatureTooltip", "Unknown function call");
	}
}

FLinearColor UNiagaraNodeFunctionCall::GetNodeTitleColor() const
{
	return UEdGraphSchema_Niagara::NodeTitleColor_FunctionCall;
}

bool UNiagaraNodeFunctionCall::CanAddToGraph(UNiagaraGraph* TargetGraph, FString& OutErrorMsg) const
{
	if (Super::CanAddToGraph(TargetGraph, OutErrorMsg) == false)
	{
		return false;
	}
	UPackage* TargetPackage = TargetGraph->GetOutermost();

	TArray<const UNiagaraGraph*> FunctionGraphs;
	UNiagaraScript* SpawningFunctionScript = FunctionScript;
	
	// We probably haven't loaded the script yet. Let's do so now so that we can trace its lineage.
	if (FunctionScriptAssetObjectPath != NAME_None && FunctionScript == nullptr)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		FAssetData ScriptAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FunctionScriptAssetObjectPath);
		if (ScriptAssetData.IsValid())
		{
			SpawningFunctionScript = Cast<UNiagaraScript>(ScriptAssetData.GetAsset());
		}
	}

	// Now we need to get the graphs referenced by the script that we are about to spawn in.
	if (SpawningFunctionScript && SpawningFunctionScript->GetSource())
	{
		UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(SpawningFunctionScript->GetSource());
		if (Source)
		{
			UNiagaraGraph* FunctionGraph = Source->NodeGraph;
			if (FunctionGraph)
			{
				FunctionGraph->GetAllReferencedGraphs(FunctionGraphs);
			}
		}
	}

	// Iterate over each graph referenced by this spawning function call and see if any of them reference the graph that we are about to be spawned into. If 
	// a match is found, then adding us would introduce a cycle and we need to abort the add.
	for (const UNiagaraGraph* Graph : FunctionGraphs)
	{
		UPackage* FunctionPackage = Graph->GetOutermost();
		if (FunctionPackage != nullptr && TargetPackage != nullptr && FunctionPackage == TargetPackage)
		{
			OutErrorMsg = LOCTEXT("NiagaraFuncCallCannotAddToGraph", "Cannot add to graph because the Function Call used by this node would lead to a cycle.").ToString();
			return false;
		}
	}

	return true;
}

UNiagaraGraph* UNiagaraNodeFunctionCall::GetCalledGraph() const
{
	if (FunctionScript)
	{
		UNiagaraScriptSource* Source = CastChecked<UNiagaraScriptSource>(FunctionScript->GetSource());
		if (Source)
		{
			UNiagaraGraph* FunctionGraph = Source->NodeGraph;
			return FunctionGraph;
		}
	}

	return nullptr;
}


ENiagaraScriptUsage UNiagaraNodeFunctionCall::GetCalledUsage() const
{
	if (FunctionScript)
	{
		return FunctionScript->GetUsage();
	}
	return ENiagaraScriptUsage::Function;
}

void UNiagaraNodeFunctionCall::Compile(class FHlslNiagaraTranslator* Translator, TArray<int32>& Outputs)
{
	TArray<int32> Inputs;

	bool bError = false;

	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());
	UNiagaraGraph* CallerGraph = GetNiagaraGraph();
	if (FunctionScript)
	{
		TArray<UEdGraphPin*> CallerInputPins;
		GetInputPins(CallerInputPins);

		UNiagaraScriptSource* Source = CastChecked<UNiagaraScriptSource>(FunctionScript->GetSource());
		UNiagaraGraph* FunctionGraph = Source->NodeGraph;

		TArray<UNiagaraNodeInput*> FunctionInputNodes;
		UNiagaraGraph::FFindInputNodeOptions Options;
		Options.bSort = true;
		Options.bFilterDuplicates = true;
		FunctionGraph->FindInputNodes(FunctionInputNodes, Options);

		for (UNiagaraNodeInput* FunctionInputNode : FunctionInputNodes)
		{
			//Finds the matching Pin in the caller.
			UEdGraphPin** PinPtr = CallerInputPins.FindByPredicate([&](UEdGraphPin* InPin) { return Schema->PinToNiagaraVariable(InPin).IsEquivalent(FunctionInputNode->Input); });
			if (!PinPtr)
			{
				if (FunctionInputNode->IsExposed())
				{
					//Couldn't find the matching pin for an exposed input. Probably a stale function call node that needs to be refreshed.
					Translator->Error(LOCTEXT("StaleFunctionCallError", "Function call is stale and needs to be refreshed."), this, nullptr);
					bError = true;
				}
				else if (FunctionInputNode->ExposureOptions.bRequired == true)
				{
					// Not exposed, but required. This means we should just add as a constant.
					Inputs.Add(Translator->GetConstant(FunctionInputNode->Input));
					continue;
				}


				Inputs.Add(INDEX_NONE);
				continue;
			}

			UEdGraphPin* CallerPin = *PinPtr;
			UEdGraphPin* CallerLinkedTo = CallerPin->LinkedTo.Num() > 0 ? UNiagaraNode::TraceOutputPin(CallerPin->LinkedTo[0]) : nullptr;			
			FNiagaraVariable PinVar = Schema->PinToNiagaraVariable(CallerPin);
			if (!CallerLinkedTo)
			{
				//if (Translator->CanReadAttributes())
				{
					//Try to auto bind if we're not linked to by the caller.
					FNiagaraVariable AutoBoundVar;
					ENiagaraInputNodeUsage AutBoundUsage = ENiagaraInputNodeUsage::Undefined;
					if (FindAutoBoundInput(FunctionInputNode, CallerPin, AutoBoundVar, AutBoundUsage))
					{
						UNiagaraNodeInput* NewNode = NewObject<UNiagaraNodeInput>(CallerGraph);
						NewNode->Input = PinVar;
						NewNode->Usage = AutBoundUsage;
						NewNode->AllocateDefaultPins();
						CallerLinkedTo = NewNode->GetOutputPin(0);
						CallerPin->BreakAllPinLinks();
						CallerPin->MakeLinkTo(CallerLinkedTo);
					}
				}
			}

			if (CallerLinkedTo)
			{
				//Param is provided by the caller. Typical case.
				Inputs.Add(Translator->CompilePin(CallerPin));
				continue;
			}
			else
			{
				if (FunctionInputNode->IsRequired())
				{
					if (CallerPin->bDefaultValueIsIgnored)
					{
						//This pin can't use a default and it is required so flag an error.
						Translator->Error(FText::Format(LOCTEXT("RequiredInputUnboundErrorFmt", "Required input {0} was not bound and could not be automatically bound."), CallerPin->GetDisplayName()),
							this, CallerPin);
						bError = true;
						//We weren't linked to anything and we couldn't auto bind so tell the compiler this input isn't provided and it should use it's local default.
						Inputs.Add(INDEX_NONE);
					}
					else
					{
						//We also compile the pin anyway if it is required as we'll be attempting to use it's inline default.
						Inputs.Add(Translator->CompilePin(CallerPin));
					}
				}
				else
				{
					//We optional, weren't linked to anything and we couldn't auto bind so tell the compiler this input isn't provided and it should use it's local default.
					Inputs.Add(INDEX_NONE);
				}
			}
		}
	}
	else if (Signature.IsValid())
	{
		bError = CompileInputPins(Translator, Inputs);
	}		
	else
	{
		Translator->Error(FText::Format(LOCTEXT("UnknownFunction", "Unknown Function Call! Missing Script or Data Interface Signature. Stack Name: {0}"), FText::FromString(GetFunctionName())), this, nullptr);
		bError = true;
	}

	if (!bError)
	{
		Translator->FunctionCall(this, Inputs, Outputs);
	}
}

bool UNiagaraNodeFunctionCall::IsValidPinToCompile(UEdGraphPin* Pin) const
{
	return !IsAddPin(Pin);
}

UObject*  UNiagaraNodeFunctionCall::GetReferencedAsset() const
{
	if (FunctionScript && FunctionScript->GetOutermost() != GetOutermost())
	{
		return FunctionScript;
	}
	else
	{
		return nullptr;
	}
}

bool UNiagaraNodeFunctionCall::RefreshFromExternalChanges()
{
	bool bReload = false;
	if (FunctionScript)
	{
		UNiagaraScriptSource* Source = CastChecked<UNiagaraScriptSource>(FunctionScript->GetSource());
		if (Source != nullptr)
		{
			bReload = CachedChangeId != Source->NodeGraph->GetChangeID();
			if (bReload)
			{
				bReload = true;
			}
		}
	}
	else
	{
		if (Signature.IsValid())
		{
			bReload = true;
		}
	}

	if (bReload)
	{
		// TODO - Leverage code in reallocate pins to determine if any pins have changed...
		ReallocatePins();
		return true;
	}
	else
	{
		return false;
	}
}

void UNiagaraNodeFunctionCall::SubsumeExternalDependencies(TMap<const UObject*, UObject*>& ExistingConversions)
{
	if (FunctionScript && FunctionScript->GetOutermost() != this->GetOutermost())
	{
		if (ExistingConversions.Contains(FunctionScript))
		{
			FunctionScript = CastChecked<UNiagaraScript>(ExistingConversions[FunctionScript]);
			check(FunctionScript->HasAnyFlags(RF_Standalone) == false);
			check(FunctionScript->HasAnyFlags(RF_Public) == false);
		}
		else
		{
			FunctionScript = FunctionScript->MakeRecursiveDeepCopy(this, ExistingConversions);
		}
	}
}

void UNiagaraNodeFunctionCall::GatherExternalDependencyIDs(ENiagaraScriptUsage InMasterUsage, const FGuid& InMasterUsageId, TArray<FGuid>& InReferencedIDs, TArray<UObject*>& InReferencedObjs) const
{
	if (FunctionScript && FunctionScript->GetOutermost() != this->GetOutermost())
	{
		UNiagaraScriptSource* Source = CastChecked<UNiagaraScriptSource>(FunctionScript->GetSource());
		UNiagaraGraph* FunctionGraph = CastChecked<UNiagaraGraph>(Source->NodeGraph);
		
		// We don't know which graph type we're referencing, so we try them all... may need to replace this with something faster in the future.
		if (FunctionGraph)
		{
			for (int32 i = (int32)ENiagaraScriptUsage::Function; i <= (int32)ENiagaraScriptUsage::Module; i++)
			{
				FGuid FoundGuid = FunctionGraph->GetCompileID((ENiagaraScriptUsage)i, FGuid(0, 0, 0, 0));
				if (FoundGuid.IsValid())
				{
					InReferencedIDs.Add(FoundGuid);
					InReferencedObjs.Add(FunctionGraph);

					FunctionGraph->GatherExternalDependencyIDs((ENiagaraScriptUsage)i, FGuid(0, 0, 0, 0), InReferencedIDs, InReferencedObjs);
				}
			}
		}
	}
}

bool UNiagaraNodeFunctionCall::ScriptIsValid() const
{
	if (FunctionScript != nullptr)
	{
		return true;
	}
	else if (Signature.IsValid())
	{
		return true;
	}
	return false;
}

void UNiagaraNodeFunctionCall::BuildParameterMapHistory(FNiagaraParameterMapHistoryBuilder& OutHistory, bool bRecursive)
{
	Super::BuildParameterMapHistory(OutHistory, bRecursive);
	if (!IsNodeEnabled() && OutHistory.GetIgnoreDisabled())
	{
		RouteParameterMapAroundMe(OutHistory, bRecursive);
		return;
	}

	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());
	if (FunctionScript)
	{
		UNiagaraScriptSource* Source = CastChecked<UNiagaraScriptSource>(FunctionScript->GetSource());
		UNiagaraGraph* FunctionGraph = CastChecked<UNiagaraGraph>(Source->NodeGraph);
		
		UNiagaraNodeOutput* OutputNode = FunctionGraph->FindOutputNode(ENiagaraScriptUsage::Function);
		if (OutputNode == nullptr)
		{
			OutputNode = FunctionGraph->FindOutputNode(ENiagaraScriptUsage::Module);
		}
		if (OutputNode == nullptr)
		{
			OutputNode = FunctionGraph->FindOutputNode(ENiagaraScriptUsage::DynamicInput);
		}

		int32 ParamMapIdx = INDEX_NONE;
		uint32 NodeIdx = INDEX_NONE;
		const UEdGraphPin* CandidateParamMapPin = GetInputPin(0);
		if (CandidateParamMapPin && CandidateParamMapPin->LinkedTo.Num() != 0 && Schema->PinToTypeDefinition(CandidateParamMapPin) == FNiagaraTypeDefinition::GetParameterMapDef())
		{
			if (bRecursive)
			{
				ParamMapIdx = OutHistory.TraceParameterMapOutputPin(UNiagaraNode::TraceOutputPin(GetInputPin(0)->LinkedTo[0]));
			}
		}
		
		OutHistory.EnterFunction(GetFunctionName(), FunctionScript, this);
		if (ParamMapIdx != INDEX_NONE)
		{
			NodeIdx = OutHistory.BeginNodeVisitation(ParamMapIdx, this);
		}
		OutputNode->BuildParameterMapHistory(OutHistory, true);

		// Since we're about to lose the pin calling context, we finish up the function call parameter map pin wiring
		// here when we have the calling context and the child context still available to us...
		TArray<UEdGraphPin*> OutputPins;
		GetOutputPins(OutputPins);

		TArray<TPair<UEdGraphPin*, int32> > MatchedPairs;
		
		// Find the matches of names and types of the sub-graph output pins and this function call nodes' outputs.
		for (UEdGraphPin* ChildOutputNodePin : OutputNode->GetAllPins())
		{
			FNiagaraVariable VarChild = Schema->PinToNiagaraVariable(ChildOutputNodePin);
			
			if (ChildOutputNodePin->LinkedTo.Num() > 0 && VarChild.GetType() == FNiagaraTypeDefinition::GetParameterMapDef())
			{
				for (int32 i = 0; i < OutputPins.Num(); i++)
				{
					FNiagaraVariable OutputVar = Schema->PinToNiagaraVariable(OutputPins[i]);
					if (OutputVar.IsEquivalent(VarChild))
					{
						TPair<UEdGraphPin*, int32> Pair;
						Pair.Key = OutputPins[i];
						Pair.Value = OutHistory.TraceParameterMapOutputPin(UNiagaraNode::TraceOutputPin(ChildOutputNodePin->LinkedTo[0]));
						MatchedPairs.Add(Pair);
					}
				}
			}
		}

		if (ParamMapIdx != INDEX_NONE)
		{
			OutHistory.EndNodeVisitation(ParamMapIdx, NodeIdx);
		}

		OutHistory.ExitFunction(GetFunctionName(), FunctionScript, this);

		for (int32 i = 0; i < MatchedPairs.Num(); i++)
		{
			OutHistory.RegisterParameterMapPin(MatchedPairs[i].Value, MatchedPairs[i].Key);
		}
	}
	else if (!ScriptIsValid())
	{
		RouteParameterMapAroundMe(OutHistory, bRecursive);
	}
}

UEdGraphPin* UNiagaraNodeFunctionCall::FindParameterMapDefaultValuePin(const FName VariableName, ENiagaraScriptUsage InParentUsage) const
{
	if (FunctionScript)
	{
		UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(FunctionScript->GetSource());
		if (ScriptSource != nullptr && ScriptSource->NodeGraph != nullptr)
		{
			return ScriptSource->NodeGraph->FindParameterMapDefaultValuePin(VariableName, FunctionScript->GetUsage(), InParentUsage);
		}
	}
	return nullptr;
}

void UNiagaraNodeFunctionCall::SuggestName(FString SuggestedName)
{
	ComputeNodeName(SuggestedName);
}

UNiagaraNodeFunctionCall::FOnInputsChanged& UNiagaraNodeFunctionCall::OnInputsChanged()
{
	return OnInputsChangedDelegate;
}

ENiagaraNumericOutputTypeSelectionMode UNiagaraNodeFunctionCall::GetNumericOutputTypeSelectionMode() const
{
	if (FunctionScript)
	{
		return FunctionScript->NumericOutputTypeSelectionMode;
	}	
	return ENiagaraNumericOutputTypeSelectionMode::None;
}

void UNiagaraNodeFunctionCall::AutowireNewNode(UEdGraphPin* FromPin)
{
	UNiagaraNode::AutowireNewNode(FromPin);
	ComputeNodeName();
}

void UNiagaraNodeFunctionCall::ComputeNodeName(FString SuggestedName)
{
	FString FunctionName = FunctionScript ? FunctionScript->GetName() : Signature.GetName();
	FName ProposedName;
	if (SuggestedName.IsEmpty() == false)
	{ 
		// If we have a suggested name and and either there is no function name, or it is a permutation of the function name
		// it can be used as the proposed name.
		if (FunctionName.IsEmpty() || SuggestedName == FunctionName || (SuggestedName.StartsWith(FunctionName) && SuggestedName.RightChop(FunctionName.Len()).IsNumeric()))
		{
			ProposedName = *SuggestedName;
		}
	}

	if(ProposedName == NAME_None)
	{
		const FString CurrentName = FunctionDisplayName;
		if (FunctionName.IsEmpty() == false)
		{
			ProposedName = *FunctionName;
		}
		else
		{
			ProposedName = *CurrentName;
		}
	}

	UNiagaraGraph* Graph = GetNiagaraGraph();
	TArray<UNiagaraNodeFunctionCall*> Nodes;
	Graph->GetNodesOfClass(Nodes);

	TSet<FName> Names;
	for (UNiagaraNodeFunctionCall* Node : Nodes)
	{
		CA_ASSUME(Node != nullptr);
		if (Node != this)
		{
			Names.Add(*Node->GetFunctionName());
		}
	}

	FString NewName = FNiagaraUtilities::GetUniqueName(ProposedName, Names).ToString();
	if (!FunctionDisplayName.Equals(NewName))
	{
		FunctionDisplayName = NewName;
	}
}

void UNiagaraNodeFunctionCall::SetPinAutoGeneratedDefaultValue(UEdGraphPin& FunctionInputPin, UNiagaraNodeInput& FunctionScriptInputNode)
{
	if (FunctionInputPin.bDefaultValueIsIgnored == false)
	{
		TArray<UEdGraphPin*> InputPins;
		FunctionScriptInputNode.GetInputPins(InputPins);
		if (InputPins.Num() == 1 && InputPins[0]->bDefaultValueIsIgnored == false)
		{
			// If the function graph's input node had an input pin, and that pin's default wasn't ignored, use that value. 
			FunctionInputPin.AutogeneratedDefaultValue = InputPins[0]->DefaultValue;
		}
		else
		{
			const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());
			FString PinDefaultValue;
			if (Schema->TryGetPinDefaultValueFromNiagaraVariable(FunctionScriptInputNode.Input, PinDefaultValue))
			{
				FunctionInputPin.AutogeneratedDefaultValue = PinDefaultValue;
			}
		}
	}
}

bool UNiagaraNodeFunctionCall::FindAutoBoundInput(UNiagaraNodeInput* InputNode, UEdGraphPin* PinToAutoBind, FNiagaraVariable& OutFoundVar, ENiagaraInputNodeUsage& OutNodeUsage)
{
	check(InputNode && InputNode->IsExposed());
	if (PinToAutoBind->LinkedTo.Num() > 0 || !InputNode->CanAutoBind())
		return false;

	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());
	FNiagaraVariable PinVar = Schema->PinToNiagaraVariable(PinToAutoBind);

	//See if we can auto bind this pin to something in the caller script.
	UNiagaraGraph* CallerGraph = GetNiagaraGraph();
	check(CallerGraph);
	UNiagaraNodeOutput* CallerOutputNodeSpawn = CallerGraph->FindOutputNode(ENiagaraScriptUsage::ParticleSpawnScript);
	UNiagaraNodeOutput* CallerOutputNodeUpdate = CallerGraph->FindOutputNode(ENiagaraScriptUsage::ParticleUpdateScript);

	//First, lest see if we're an attribute of this emitter. Only valid if we're a module call off the primary script.
	if (CallerOutputNodeSpawn || CallerOutputNodeUpdate)
	{
		UNiagaraNodeOutput* CallerOutputNode = CallerOutputNodeSpawn != nullptr ? CallerOutputNodeSpawn : CallerOutputNodeUpdate;
		check(CallerOutputNode);
		{
			FNiagaraVariable* AttrVarPtr = CallerOutputNode->Outputs.FindByPredicate([&](const FNiagaraVariable& Attr) { return PinVar.IsEquivalent(Attr); });
			if (AttrVarPtr)
			{
				OutFoundVar = *AttrVarPtr;
				OutNodeUsage = ENiagaraInputNodeUsage::Attribute;
				return true;
			}
		}
	}
	
	//Next, lets see if we are a system constant.
	//Do we need a smarter (possibly contextual) handling of system constants?
	const TArray<FNiagaraVariable>& SysConstants = FNiagaraConstants::GetEngineConstants();
	if (SysConstants.Contains(PinVar))
	{
		OutFoundVar = PinVar;
		OutNodeUsage = ENiagaraInputNodeUsage::SystemConstant;
		return true;
	}

	//Not sure it's a good idea to allow binding to user made parameters.
// 	if (AutoBindOptions.bBindToParameters)
// 	{
// 		//Finally, lets see if we're a parameter of this emitter.
// 		TArray<UNiagaraNodeInput*> CallerInputNodes;
// 		CallerGraph->FindInputNodes(CallerInputNodes);
// 		UNiagaraNodeInput** MatchingParamPtr = CallerInputNodes.FindByPredicate([&](UNiagaraNodeInput*& CallerInputNode)
// 		{
// 			return CallerInputNode->Input.IsEquivalent(PinVar);
// 		});
// 
// 		if (MatchingParamPtr)
// 		{
// 			OutFoundVar = (*MatchingParamPtr)->Input;
// 			OutNodeUsage = ENiagaraInputNodeUsage::Parameter;
// 			return true;
// 		}
// 	}

	//Unable to auto bind.
	return false;
}

#undef LOCTEXT_NAMESPACE