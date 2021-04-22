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
/**
 * Implements generators for synthesizing mostly syntactically valid
 * Solidity test programs.
 */

#pragma once

#include <test/tools/ossfuzz/Generators.h>

#include <liblangutil/Exceptions.h>

#include <memory>
#include <random>
#include <set>
#include <variant>

namespace solidity::test::fuzzer::mutator
{
/// Forward declarations
class SolidityGenerator;

/// Type declarations
#define SEMICOLON() ;
#define FORWARDDECLAREGENERATORS(G) class G
GENERATORLIST(FORWARDDECLAREGENERATORS, SEMICOLON(), SEMICOLON())
#undef FORWARDDECLAREGENERATORS
#undef SEMICOLON

#define COMMA() ,
using GeneratorPtr = std::variant<
#define VARIANTOFSHARED(G) std::shared_ptr<G>
GENERATORLIST(VARIANTOFSHARED, COMMA(), )
>;
#undef VARIANTOFSHARED
using Generator = std::variant<
#define VARIANTOFGENERATOR(G) G
GENERATORLIST(VARIANTOFGENERATOR, COMMA(), )
>;
#undef VARIANTOFGENERATOR
#undef COMMA
using RandomEngine = std::mt19937_64;
using Distribution = std::uniform_int_distribution<size_t>;

struct UniformRandomDistribution
{
	explicit UniformRandomDistribution(std::unique_ptr<RandomEngine> _randomEngine):
		randomEngine(std::move(_randomEngine))
	{}

	/// @returns an unsigned integer in the range [1, @param _n] chosen
	/// uniformly at random.
	[[nodiscard]] size_t distributionOneToN(size_t _n) const
	{
		solAssert(_n > 0, "");
		return Distribution(1, _n)(*randomEngine);
	}
	/// @returns true with a probability of 1/(@param _n), false otherwise.
	/// @param _n > 1.
	[[nodiscard]] bool probable(size_t _n) const
	{
		solAssert(_n > 1, "");
		return distributionOneToN(_n) == 1;
	}
	/// @returns true with a probability of 1 - 1/(@param _n),
	/// false otherwise.
	/// @param _n > 1.
	[[nodiscard]] bool likely(size_t _n) const
	{
		solAssert(_n > 1, "");
		return !probable(_n);
	}
	/// @returns a subset whose elements are of type @param T
	/// created from the set @param _container using
	/// uniform selection.
	template <typename T>
	std::set<T> subset(std::set<T> const& _container)
	{
		size_t s = _container.size();
		solAssert(s > 1, "");
		std::set<T> subContainer;
		for (auto const& item: _container)
			if (probable(s))
				subContainer.insert(item);
		return subContainer;
	}
	std::unique_ptr<RandomEngine> randomEngine;
};

struct SourceState
{
	explicit SourceState(std::shared_ptr<UniformRandomDistribution> _urd):
		uRandDist(std::move(_urd)),
		importedSources({})
	{}
	void addImportedSourcePath(std::string& _sourcePath)
	{
		importedSources.emplace(_sourcePath);
	}
	[[nodiscard]] bool sourcePathImported(std::string const& _sourcePath) const
	{
		return importedSources.count(_sourcePath);
	}
	~SourceState()
	{
		importedSources.clear();
	}
	/// Prints source state to @param _os.
	void print(std::ostream& _os) const;
	std::shared_ptr<UniformRandomDistribution> uRandDist;
	std::set<std::string> importedSources;
};

struct TestState
{
	explicit TestState(std::shared_ptr<UniformRandomDistribution> _urd):
		sourceUnitState({}),
		currentSourceUnitPath({}),
		uRandDist(std::move(_urd)),
		numSourceUnits(0)
	{}
	/// Adds @param _path to @name sourceUnitPaths updates
	/// @name currentSourceUnitPath.
	void addSourceUnit(std::string const& _path)
	{
		sourceUnitState.emplace(_path, std::make_shared<SourceState>(uRandDist));
		currentSourceUnitPath = _path;
	}
	/// Returns true if @name sourceUnitPaths is empty,
	/// false otherwise.
	[[nodiscard]] bool empty() const
	{
		return sourceUnitState.empty();
	}
	/// Returns the number of items in @name sourceUnitPaths.
	[[nodiscard]] size_t size() const
	{
		return sourceUnitState.size();
	}
	/// Returns a new source path name that is formed by concatenating
	/// a static prefix @name m_sourceUnitNamePrefix, a monotonically
	/// increasing counter starting from 0 and the postfix (extension)
	/// ".sol".
	[[nodiscard]] std::string newPath() const
	{
		return sourceUnitNamePrefix + std::to_string(numSourceUnits) + ".sol";
	}
	[[nodiscard]] std::string currentPath() const
	{
		solAssert(numSourceUnits > 0, "");
		return currentSourceUnitPath;
	}
	/// Adds @param _path to list of source paths in global test
	/// state and increments @name m_numSourceUnits.
	void updateSourcePath(std::string const& _path)
	{
		addSourceUnit(_path);
		numSourceUnits++;
	}
	/// Adds a new source unit to test case.
	void addSource()
	{
		updateSourcePath(newPath());
	}
	~TestState()
	{
		sourceUnitState.clear();
	}
	/// Prints test state to @param _os.
	void print(std::ostream& _os) const;
	/// Returns a randomly chosen path from @param _sourceUnitPaths.
	[[nodiscard]] std::string randomPath(std::set<std::string> const& _sourceUnitPaths) const;
	[[nodiscard]] std::set<std::string> sourceUnitPaths() const;
	/// Returns a randomly chosen path from @name sourceUnitPaths.
	[[nodiscard]] std::string randomPath() const;
	/// Returns a randomly chosen non current source unit path.
	[[nodiscard]] std::string randomNonCurrentPath() const;
	/// List of source paths in test input.
	std::map<std::string, std::shared_ptr<SourceState>> sourceUnitState;
	/// Source path being currently visited.
	std::string currentSourceUnitPath;
	/// Uniform random distribution.
	std::shared_ptr<UniformRandomDistribution> uRandDist;
	/// Number of source units in test input
	size_t numSourceUnits;
	/// String prefix of source unit names
	std::string const sourceUnitNamePrefix = "su";
};

struct GeneratorBase
{
	explicit GeneratorBase(std::shared_ptr<SolidityGenerator> _mutator);
	template <typename T>
	std::shared_ptr<T> generator()
	{
		for (auto& g: generators)
			if (std::holds_alternative<std::shared_ptr<T>>(g.first))
				return std::get<std::shared_ptr<T>>(g.first);
		solAssert(false, "");
	}
	/// @returns test fragment created by this generator.
	std::string generate()
	{
		std::string generatedCode = visit();
		endVisit();
		return generatedCode;
	}
	/// @returns a string representing the generation of
	/// the Solidity grammar element.
	virtual std::string visit() = 0;
	/// Method called after visiting this generator. Used
	/// for clearing state if necessary.
	virtual void endVisit() {}
	/// Visitor that invokes child grammar elements of
	/// this grammar element returning their string
	/// representations.
	std::string visitChildren();
	/// Adds generators for child grammar elements of
	/// this grammar element.
	void addGenerators(std::set<std::pair<GeneratorPtr, unsigned>> _generators)
	{
		generators += _generators;
	}
	/// Virtual method to obtain string name of generator.
	virtual std::string name() = 0;
	/// Virtual method to add generators that this grammar
	/// element depends on. If not overridden, there are
	/// no dependencies.
	virtual void setup() {}
	virtual ~GeneratorBase()
	{
		generators.clear();
	}
	/// Shared pointer to the mutator instance
	std::shared_ptr<SolidityGenerator> mutator;
	/// Set of generators used by this generator.
	std::set<std::pair<GeneratorPtr, unsigned>> generators;
	/// Shared ptr to global test state.
	std::shared_ptr<TestState> state;
	/// Uniform random distribution
	std::shared_ptr<UniformRandomDistribution> uRandDist;
};

class TestCaseGenerator: public GeneratorBase
{
public:
	explicit TestCaseGenerator(std::shared_ptr<SolidityGenerator> _mutator):
		GeneratorBase(std::move(_mutator))
	{}
	void setup() override;
	std::string visit() override;
	std::string name() override
	{
		return "Test case generator";
	}
private:
	/// @returns a new source path name that is formed by concatenating
	/// a static prefix @name m_sourceUnitNamePrefix, a monotonically
	/// increasing counter starting from 0 and the postfix (extension)
	/// ".sol".
	[[nodiscard]] std::string path() const
	{
		return m_sourceUnitNamePrefix + std::to_string(m_numSourceUnits) + ".sol";
	}
	/// Adds @param _path to list of source paths in global test
	/// state and increments @name m_numSourceUnits.
	void updateSourcePath(std::string const& _path)
	{
		state->addSourceUnit(_path);
		m_numSourceUnits++;
	}
	/// Number of source units in test input
	size_t m_numSourceUnits;
	/// String prefix of source unit names
	std::string const m_sourceUnitNamePrefix = "su";
	/// Maximum number of source units per test input
	static constexpr unsigned s_maxSourceUnits = 3;
};

class SourceUnitGenerator: public GeneratorBase
{
public:
	explicit SourceUnitGenerator(std::shared_ptr<SolidityGenerator> _mutator):
		GeneratorBase(std::move(_mutator))
	{}
	void setup() override;
	std::string visit() override;
	std::string name() override { return "Source unit generator"; }
private:
	static unsigned constexpr s_maxImports = 2;
};

class PragmaGenerator: public GeneratorBase
{
public:
	explicit PragmaGenerator(std::shared_ptr<SolidityGenerator> _mutator):
		GeneratorBase(std::move(_mutator))
	{}
	std::string visit() override;
	std::string name() override { return "Pragma generator"; }
private:
	std::set<std::string> const s_genericPragmas = {
		R"(pragma solidity >= 0.0.0;)",
		R"(pragma experimental SMTChecker;)",
	};
	std::vector<std::string> const s_abiPragmas = {
		R"(pragma abicoder v1;)",
		R"(pragma abicoder v2;)"
	};
};

class ImportGenerator: public GeneratorBase
{
public:
	explicit ImportGenerator(std::shared_ptr<SolidityGenerator> _mutator):
	       GeneratorBase(std::move(_mutator))
	{}
	std::string visit() override;
	std::string name() override { return "Import generator"; }
};

class ContractGenerator: public GeneratorBase
{
public:
	explicit ContractGenerator(std::shared_ptr<SolidityGenerator> _mutator):
		GeneratorBase(std::move(_mutator))
	{}
	std::string visit() override;
	std::string name() override { return "Contract generator"; }
};

class SolidityGenerator: public std::enable_shared_from_this<SolidityGenerator>
{
public:
	explicit SolidityGenerator(unsigned _seed);

	/// @returns the generator of type @param T.
	template <typename T>
	std::shared_ptr<T> generator();
	/// @returns a shared ptr to underlying random
	/// number distribution.
	std::shared_ptr<UniformRandomDistribution> uniformRandomDist()
	{
		return m_urd;
	}
	/// @returns a pseudo randomly generated test case.
	std::string generateTestProgram();
	/// @returns shared ptr to global test state.
	std::shared_ptr<TestState> testState()
	{
		return m_state;
	}
private:
	template <typename T>
	void createGenerator()
	{
		m_generators.insert(
			std::make_shared<T>(shared_from_this())
		);
	}
	template <std::size_t I = 0>
	void createGenerators();
	void destroyGenerators()
	{
		m_generators.clear();
	}
	/// Sub generators
	std::set<GeneratorPtr> m_generators;
	/// Shared global test state
	std::shared_ptr<TestState> m_state;
	/// Uniform random distribution
	std::shared_ptr<UniformRandomDistribution> m_urd;
};
}
