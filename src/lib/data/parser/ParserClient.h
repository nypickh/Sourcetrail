#ifndef PARSER_CLIENT_H
#define PARSER_CLIENT_H

#include <string>
#include <vector>

#include "utility/types.h"
#include "data/name/NameHierarchy.h"

#include "utility/file/FileInfo.h"

struct ParseLocation;
class DataType;

class ParserClient
{
public:
	enum AccessType {
		ACCESS_PUBLIC,
		ACCESS_PROTECTED,
		ACCESS_PRIVATE,
		ACCESS_NONE
	};

	enum AbstractionType {
		ABSTRACTION_VIRTUAL,
		ABSTRACTION_PURE_VIRTUAL,
		ABSTRACTION_NONE
	};

	enum RecordType {
		RECORD_STRUCT,
		RECORD_CLASS
	};

	static std::string addAccessPrefix(const std::string& str, AccessType access);
	static std::string addAbstractionPrefix(const std::string& str, AbstractionType abstraction);
	static std::string addStaticPrefix(const std::string& str, bool isStatic);
	static std::string addConstPrefix(const std::string& str, bool isConst, bool atFront);
	static std::string addLocationSuffix(const std::string& str, const ParseLocation& location);
	static std::string addLocationSuffix(
		const std::string& str, const ParseLocation& location, const ParseLocation& scopeLocation);


	ParserClient();
	virtual ~ParserClient();

	virtual void startParsing() = 0;
	virtual void finishParsing() = 0;

	virtual void startParsingFile(const FilePath& filePath) = 0;
	virtual void finishParsingFile(const FilePath& filePath) = 0;

	virtual void onError(const ParseLocation& location, const std::string& message, bool fatal) = 0;

	virtual Id onTypedefParsed(
		const ParseLocation& location, const NameHierarchy& typedefName, AccessType access, bool isImplicit) = 0;
	virtual Id onClassParsed(
		const ParseLocation& location, const NameHierarchy& nameHierarchy, AccessType access,
		const ParseLocation& scopeLocation, bool isImplicit) = 0;
	virtual Id onStructParsed(
		const ParseLocation& location, const NameHierarchy& nameHierarchy, AccessType access,
		const ParseLocation& scopeLocation, bool isImplicit) = 0;
	virtual Id onGlobalVariableParsed(const ParseLocation& location, const NameHierarchy& variable, bool isImplicit) = 0;
	virtual Id onFieldParsed(const ParseLocation& location, const NameHierarchy& field, AccessType access, bool isImplicit) = 0;
	virtual Id onFunctionParsed(
		const ParseLocation& location, const NameHierarchy& function, const ParseLocation& scopeLocation, bool isImplicit) = 0;
	virtual Id onMethodParsed(
		const ParseLocation& location, const NameHierarchy& method, AccessType access, AbstractionType abstraction,
		const ParseLocation& scopeLocation, bool isImplicit) = 0;
	virtual Id onNamespaceParsed(
		const ParseLocation& location, const NameHierarchy& nameHierarchy,
		const ParseLocation& scopeLocation, bool isImplicit) = 0;
	virtual Id onEnumParsed(
		const ParseLocation& location, const NameHierarchy& nameHierarchy, AccessType access,
		const ParseLocation& scopeLocation, bool isImplicit) = 0;
	virtual Id onEnumConstantParsed(const ParseLocation& location, const NameHierarchy& nameHierarchy, bool isImplicit) = 0;
	virtual Id onTemplateParameterTypeParsed(
		const ParseLocation& location, const NameHierarchy& templateParameterTypeNameHierarchy, bool isImplicit) = 0;

	virtual Id onInheritanceParsed(
		const ParseLocation& location, const NameHierarchy& nameHierarchy,
		const NameHierarchy& baseNameHierarchy, AccessType access) = 0;
	virtual Id onMethodOverrideParsed(
		const ParseLocation& location, const NameHierarchy& overridden, const NameHierarchy& overrider) = 0;
	virtual Id onCallParsed(
		const ParseLocation& location, const NameHierarchy& caller, const NameHierarchy& callee) = 0;
	virtual Id onFieldUsageParsed(
		const ParseLocation& location, const NameHierarchy& userNameHierarchy, const NameHierarchy& usedNameHierarchy) = 0;
	virtual Id onGlobalVariableUsageParsed(
		const ParseLocation& location, const NameHierarchy& userNameHierarchy, const NameHierarchy& usedNameHierarchy) = 0;
	virtual Id onEnumConstantUsageParsed(
		const ParseLocation& location, const NameHierarchy& userNameHierarchy, const NameHierarchy& usedNameHierarchy) = 0;
	virtual Id onTypeUsageParsed(const ParseLocation& location, const NameHierarchy& user, const NameHierarchy& used) = 0;

	virtual Id onTemplateArgumentTypeParsed(
		const ParseLocation& location, const NameHierarchy& argumentTypeNameHierarchy,
		const NameHierarchy& templateNameHierarchy) = 0;
	virtual Id onTemplateDefaultArgumentTypeParsed(
		const ParseLocation& location, const NameHierarchy& defaultArgumentTypeNameHierarchy,
		const NameHierarchy& templateArgumentTypeNameHierarchy) = 0;
	virtual Id onTemplateSpecializationParsed(
		const ParseLocation& location, const NameHierarchy& specializedNameHierarchy,
		const NameHierarchy& specializedFromNameHierarchy) = 0;
	virtual Id onTemplateMemberFunctionSpecializationParsed(
		const ParseLocation& location, const NameHierarchy& instantiatedFunction, const NameHierarchy& specializedFunction) = 0;

	virtual Id onFileParsed(const FileInfo& fileInfo) = 0;
	virtual Id onFileIncludeParsed(
		const ParseLocation& location, const FileInfo& fileInfo, const FileInfo& includedFileInfo) = 0;

	virtual Id onMacroDefineParsed(
		const ParseLocation& location, const NameHierarchy& macroNameHierarchy, const ParseLocation& scopeLocation) = 0;
	virtual Id onMacroExpandParsed(
		const ParseLocation& location, const NameHierarchy& macroNameHierarchy) = 0;

	virtual Id onCommentParsed(const ParseLocation& location) = 0;
};

#endif // PARSER_CLIENT_H
