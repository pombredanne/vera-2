//
// Copyright (C) 2006-2007 Maciej Sobczak
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#include "StatementOfStruct.h"
#include "IsTokenWithName.h"
#include "StatementOfTemplateParameters.h"
#include "Document.h"

#include <vector>
#include <map>
#include <algorithm>

#define IS_NOT_TOKEN "the first element of the collection is not a token of 'struct' type."
#define WITHOUT_STATEMENT_SCOPE "The statement not contain a scope associated."
#define TOKEN_NAME             "struct"
#define IDENTIFIER_TOKEN_NAME  "identifier"
#define LEFTPAREN_TOKEN_NAME   "leftparen"
#define RIGHTPAREN_TOKEN_NAME  "rightparen"
#define LEFTBRACE_TOKEN_NAME   "leftbrace"
#define RIGHTBRACE_TOKEN_NAME  "rightbrace"
#define LESS_TOKEN_NAME       "less"
#define GREATER_TOKEN_NAME    "greater"
#define SEMICOLON_TOKEN_NAME   "semicolon"
#define COLON_TOKEN_NAME       "colon"
#define IS_EQUAL_RETURN(left, right) \
  {\
    if (left == right) \
    { \
      return;\
    }\
  }
#define IS_EQUAL_RETURN_FALSE(left, right) \
  {\
    if (left == right) \
    { \
      return false;\
    }\
  }
#define IS_EQUAL_BREAK(left, right) \
  {\
    if (left == right) \
    { \
      break;\
    }\
  }

namespace Vera
{
namespace Structures
{

StatementOfStruct::StatementOfStruct(Statement& statement,
  Tokens::TokenSequence::const_iterator& it,
  Tokens::TokenSequence::const_iterator& end)
: StatementsBuilder(statement)
, hieritance_(NULL)
, variables_(NULL)
, id_(statement.id_)
{
  statement.type_ = Statement::TYPE_ITEM_STATEMENT_OF_STRUCT_UNNAME;

  name_ = getDefaultName();

  initialize(it, end);
}

void
StatementOfStruct::initialize(Tokens::TokenSequence::const_iterator& it,
    Tokens::TokenSequence::const_iterator& end)
{
  push(*it);
  ++it;

  addEachInvalidToken(it, end);
}

bool
StatementOfStruct::parseName(Tokens::TokenSequence::const_iterator& it,
    Tokens::TokenSequence::const_iterator& end)
{
  bool successful = false;

  IS_EQUAL_RETURN_FALSE(it, end);

  Statement& current = getCurrentStatement();

  current.type_ = Statement::TYPE_ITEM_STATEMENT_OF_STRUCT;

  if (IsTokenWithName(IDENTIFIER_TOKEN_NAME)(*it) == true )
  {
    push(*it);

    name_ = it->value_;
    ++it;
    IS_EQUAL_RETURN_FALSE(it, end);
    addEachInvalidToken(it, end);
    successful = true;
  }

  return successful;
}

bool
StatementOfStruct::parseHeritage(Tokens::TokenSequence::const_iterator& it,
    Tokens::TokenSequence::const_iterator& end)
{
  bool successful = false;

  IS_EQUAL_RETURN_FALSE(it, end);

  if (IsTokenWithName(COLON_TOKEN_NAME)(*it) == true )
  {
    StatementsBuilder partial(add());

    successful = partial.parseHeritage(it, end);
  }

  if (successful)
  {
    hieritance_ = &getCurrentStatement().statementSequence_.back();
    hieritance_->type_ = Statement::TYPE_ITEM_STATEMENT_OF_HERITAGE;
  }

  return successful;
}

bool
StatementOfStruct::parseScope(Tokens::TokenSequence::const_iterator& it,
    Tokens::TokenSequence::const_iterator& end)
{
  bool successful = false;

  IS_EQUAL_RETURN_FALSE(it, end);
  if (IsTokenWithName(LEFTBRACE_TOKEN_NAME)(*it) == false )
  {
    return false;
  }

  Statement& current = add();
  current.type_ = Statement::TYPE_ITEM_STATEMENT_OF_BRACESARGUMENTS;
  StatementsBuilder branch(current);

  branch.push(*it);

  it++;

  Tokens::TokenSequence::const_iterator rightBraceMached = std::find_if(it,
    end,
    EndsWithCorrectPattern(LEFTBRACE_TOKEN_NAME, RIGHTBRACE_TOKEN_NAME));

  branch.addEachInvalidToken(it, end);

  while (it < rightBraceMached)
  {
    branch.builder(it, rightBraceMached);

    Statement& lastItem = current.getBack();

    if (isSignature(lastItem))
    {
      lastItem.type_ = Statement::TYPE_ITEM_STATEMENT_OF_SIGNATURE_DECLARATION;
    }

    ++it;
    branch.addEachInvalidToken(it, rightBraceMached);
  }

  if (it >= end)
  {
    return false;
  }

  if (IsTokenWithName(RIGHTBRACE_TOKEN_NAME)(*it) == true)
  {
    successful = true;

    branch.push(*it);
    ++it;
  }

  return successful;
}

bool
StatementOfStruct::parseVariablesFromScopeToSemicolon(Tokens::TokenSequence::const_iterator& it,
    Tokens::TokenSequence::const_iterator& end)
{
  bool successful = false;

  Statement& current = getCurrentStatement();

  addEachInvalidToken(it, end);

  if (StatementsBuilder::parseVariablesFromScopeToSemicolon(it, end))
  {
    variables_ = &current.statementSequence_.back();
    //variables_->type_ = Statement::TYPE_ITEM_STATEMENT_OF_DECLARATION_LIST;
    successful = true;
  }

  return successful;
}

bool
StatementOfStruct::isValidWithoutName(Tokens::TokenSequence::const_iterator it,
  Tokens::TokenSequence::const_iterator end)
{
  if (it->name_.compare(TOKEN_NAME) != 0)
  {
    return false;
  }

  Tokens::TokenSequence::const_iterator itMatched = std::find_if(it+1,
    end,
    IsValidTokenForStatement());

  IS_EQUAL_RETURN_FALSE(itMatched, end)

  if (IsTokenWithName(LEFTBRACE_TOKEN_NAME)(*itMatched) == true)
  {
    return true;
  }

  if (IsTokenWithName(COLON_TOKEN_NAME)(*itMatched) == false)
  {
    return false;
  }

  Statement auxiliar;

  StatementsBuilder partial(auxiliar);

  if (partial.parseHeritage(itMatched, end) == false)
  {
    return false;
  }

  IS_EQUAL_RETURN_FALSE(itMatched, end)

  itMatched = std::find_if(itMatched,
      end,
      IsValidTokenForStatement());

  IS_EQUAL_RETURN_FALSE(itMatched, end)

  if (IsTokenWithName(LEFTBRACE_TOKEN_NAME)(*itMatched) == true)
  {
    return true;
  }

  return false;
}

bool
StatementOfStruct::isValidWithName(Tokens::TokenSequence::const_iterator it,
  Tokens::TokenSequence::const_iterator end)
{
  if (it->name_.compare(TOKEN_NAME) != 0)
  {
    return false;
  }

  Tokens::TokenSequence::const_iterator itMatched = std::find_if(it+1,
    end,
    IsValidTokenForStatement());

  IS_EQUAL_RETURN_FALSE(itMatched, end)

  if (IsTokenWithName(IDENTIFIER_TOKEN_NAME)(*itMatched) == false)
  {
    return false;
  }

  ++itMatched;
  IS_EQUAL_RETURN_FALSE(itMatched, end)

  itMatched = std::find_if(itMatched,
      end,
      IsValidTokenForStatement());

  if (StatementOfTemplateParameters::isValid(itMatched, end) == true)
  {
    ++itMatched;
    itMatched = std::find_if(itMatched,
      end,
      EndsWithCorrectPattern(LESS_TOKEN_NAME, GREATER_TOKEN_NAME));

    ++itMatched;

    itMatched = std::find_if(itMatched,
        end,
        IsValidTokenForStatement());
  }

  if (IsTokenWithName(LEFTBRACE_TOKEN_NAME)(*itMatched) == true)
  {
    return true;
  }

  if (IsTokenWithName(COLON_TOKEN_NAME)(*itMatched) == false)
  {
    return false;
  }


  Statement auxiliar;
  StatementsBuilder partial(auxiliar);

  if (partial.parseHeritage(itMatched, end) == false)
  {
    return false;
  }

  IS_EQUAL_RETURN_FALSE(itMatched, end)

  itMatched = std::find_if(itMatched,
      end,
      IsValidTokenForStatement());

  if (IsTokenWithName(LEFTBRACE_TOKEN_NAME)(*itMatched) == true)
  {
    return true;
  }

  return false;
}

const std::string&
StatementOfStruct::getName()
{
  return name_;
}

std::size_t
StatementOfStruct::getId()
{
  return id_;
}

bool
StatementOfStruct::isValid(Tokens::TokenSequence::const_iterator it,
  Tokens::TokenSequence::const_iterator end)
{
  return isValidWithName(it, end) || isValidWithoutName(it, end);
}

bool
StatementOfStruct::create(Statement& statement,
  Tokens::TokenSequence::const_iterator& it,
  Tokens::TokenSequence::const_iterator& end)
{
  bool successful = false;
  bool isValid = false;
  bool hasName = false;

  if (isValidWithName(it, end))
  {
    hasName = true;
    isValid = true;
  }
  else
  {
    isValid = true;
  }

  if (isValid)
  {
    StatementOfStruct builder(statement.add(), it, end);

    if (hasName)
    {
      builder.parseName(it, end);

      if (StatementOfTemplateParameters::isValid(it, end))
      {
        StatementOfTemplateParameters(builder.add(), it, end);

        builder.addEachInvalidToken(it, end);
      }
    }

    builder.parseHeritage(it, end);
    builder.parseScope(it, end);
    builder.parseVariablesFromScopeToSemicolon(it, end);

    statement.doc_->addStruct(builder.getName(), builder.getId());

    successful = true;
  }

  return successful;
}

} // Vera namespace
} // Structures namespace

