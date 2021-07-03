## README.md

## OVRRootPath.propa
<xml version="1.0" encoding="utf-8"?>
<--
To load this file your .vcxproj should have the following Import tag before all others:
<Import Project="$([MSBuild]::GetDirectoryNameOfFileAbove($(MSBuildThisFileDirectory), OVRRootPath.props))\OVRRootPath.props" />
Unfortunately there is currently no way to add this via the VS GUI, you'll have to edit
new .vcxproj files manually or copy an existing project.
-->
<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ImportGroup Label="PropertySheets" />
  <PropertyGroup Label="UserMacros">
    <OVRSDKROOT>$(MSBuildThisFileDirectory)</OVRSDKROOT>
    <VSDIR Condition="'$(PlatformToolset)'=='v120'">VS2013</VSDIR>
    <VSDIR Condition="'$(PlatformToolset)'=='v140'">VS2015</VSDIR>
  </PropertyGroup>
  <PropertyGroup />
  <ItemDefinitionGroup />
  <ItemGroup>
    <BuildMacro Include="OVRSDKROOT">
      <Value>$(OVRSDKROOT)</Value>
      <EnvironmentVariable>true</EnvironmentVariable>
    </BuildMacro>
    <BuildMacro Include="VSDIR">
      <Value>$(VSDIR)</Value>
      <EnvironmentVariable>true</EnvironmentVariable>
    </BuildMacro>
  </ItemGroup>
</Project>
