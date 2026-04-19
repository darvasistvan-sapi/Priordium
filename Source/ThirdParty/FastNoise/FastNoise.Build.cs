// Copyright Priordium. All Rights Reserved.
//
// FastNoise.Build.cs
// UE5 ThirdParty modul a FastNoiseLite header-only könyvtárhoz.
// Referencia: https://github.com/Auburn/FastNoiseLite (MIT License)
//
// MEGJEGYZÉS: Type = ModuleType.External modulokat NEM lehet PrivateDependencyModuleNames-be tenni.
// A fogyaszto Build.cs (Priordium.Build.cs) kozvetlenul hivatkozza be az include path-t.
// Ez a fájl dokumentációs és struktúra célokat szolgál.

using UnrealBuildTool;
using System.IO;

public class FastNoise : ModuleRules
{
	public FastNoise(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// Header-only könyvtár: csak az include path szükséges, nincs .lib/.dll
		PublicIncludePaths.Add(ModuleDirectory);

		// Jelző, hogy a FastNoiseLite elérhető – feltételes fordítás támogatása
		PublicDefinitions.Add("WITH_FASTNOISE=1");
	}
}
