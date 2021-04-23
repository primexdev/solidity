/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0

#include <libyul/optimiser/UnusedFunctionsCommon.h>

#include <libyul/Dialect.h>

#include <liblangutil/SourceLocation.h>

#include <libsolutil/CommonData.h>

using namespace solidity;
using namespace solidity::util;
using namespace solidity::yul;
using namespace solidity::yul::unusedFunctionsCommon;

FunctionDefinition unusedFunctionsCommon::createLinkingFunction(
	FunctionDefinition const& _original,
	std::pair<std::vector<bool>, std::vector<bool>> const& _usedParametersAndReturns,
	YulString const& _originalFunctionName,
	YulString const& _linkingFunctionName,
	NameDispenser& _nameDispenser
)
{
	auto generateTypedName = [&](TypedName t)
	{
		return TypedName{
			t.debugData,
			_nameDispenser.newName(t.name),
			t.type
		};
	};

	std::shared_ptr<DebugData const> debugData = _original.debugData;

	FunctionDefinition linkingFunction{
		debugData,
		_linkingFunctionName,
		util::applyMap(_original.parameters, generateTypedName),
		util::applyMap(_original.returnVariables, generateTypedName),
		{debugData, {}} // body
	};

	FunctionCall call{debugData, Identifier{debugData, _originalFunctionName}, {}};
	for (auto const& p: filter(linkingFunction.parameters, _usedParametersAndReturns.first))
		call.arguments.emplace_back(Identifier{debugData, p.name});

	Assignment assignment{debugData, {}, nullptr};

	for (auto const& r: filter(linkingFunction.returnVariables, _usedParametersAndReturns.second))
		assignment.variableNames.emplace_back(Identifier{debugData, r.name});

	if (assignment.variableNames.empty())
		linkingFunction.body.statements.emplace_back(ExpressionStatement{debugData, std::move(call)});
	else
	{
		assignment.value = std::make_unique<Expression>(std::move(call));
		linkingFunction.body.statements.emplace_back(std::move(assignment));
	}

	return linkingFunction;
}
