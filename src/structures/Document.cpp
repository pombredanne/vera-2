//
// Copyright (C) 2006-2007 Maciej Sobczak
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#include "Document.h"
#include "IsTokenWithName.h"
#include "TokensIterator.h"
#include "SourceFiles.h"
#include "../plugins/Messages.h"

#include <vector>
#include <map>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/function.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <unistd.h>
#include <boost/asio/io_service.hpp>

#define SYSTEM_INCLUDE_LABEL "sysInclude"
#define INCLUDE_LABEL "include"
#define MACRO_LABEL "macro"

#define EOF_TOKEN_NAME  "eof"

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
namespace
{

std::vector<std::string> includes_;
std::vector<std::string> sysIncludes_;
std::vector<std::string> preDefines_;
std::vector<std::string> types_;
std::vector<std::string> structures_;
typedef std::map<std::string, boost::shared_ptr<Document> > DocumentSequence;
typedef std::map<std::size_t, StatementOfDefine> StatementOfDefineSequence;
typedef std::map<std::size_t, StatementOfStruct> StatementOfStructSequence;
typedef std::map<std::size_t, StatementOfEnum> StatementOfEnumSequence;
typedef std::map<std::size_t, StatementOfUnion> StatementOfUnionSequence;
DocumentSequence documents_;
StatementOfDefineSequence defines_;
StatementOfEnumSequence enums_;
StatementOfUnionSequence unions_;

} // unname namespace

void
Documents::initialize()
{
  boost::asio::io_service ioService;
  boost::asio::io_service::work work(ioService);

  const SourceFiles::FileNameSet& files = SourceFiles::getAllFileNames();

  boost::thread_group threadPool;

  //TODO: Pending improvement to handle the pool thread.

  threadPool.create_thread(
      boost::bind(&boost::asio::io_service::run, &ioService)
  );

  threadPool.create_thread(
      boost::bind(&boost::asio::io_service::run, &ioService)
  );

  threadPool.create_thread(
      boost::bind(&boost::asio::io_service::run, &ioService)
  );

  threadPool.create_thread(
      boost::bind(&boost::asio::io_service::run, &ioService)
  );

  threadPool.create_thread(
      boost::bind(&boost::asio::io_service::run, &ioService)
  );

  for (SourceFiles::FileNameSet::const_iterator it = files.begin();
      it != files.end(); ++it)
  {
    std::string fileName = *it;
    Tokens::TokenSequence tokens = Tokens::getEachTokenFromFile(fileName);

    ioService.post(boost::bind(&Documents::parse, this, fileName));
  }

  ioService.stop();

  threadPool.join_all();
}

void
Documents::parse(std::string fileName)
{
  boost::shared_ptr<Document> doc = Document::create (fileName);

  doc->initialize();
}

Document::Document(const std::string& name)
    : fileName_(name)
        , root_(Token("root", 0, 0, "unknown"))
        , wasInitialized_(false)
, isFunction_(false)
, isUnion_(false)
{
  root_.parentId_ = root_.id_;
  root_.doc_ = this;
  root_.type_ = Statement::TYPE_ITEM_ROOT;

  paths_.set_current_directory(name.c_str());

  boost::filesystem::path path = paths_.get_current_directory();

  std::stringstream out;

  out << "Path: " << path.c_str() << std::endl;

  addIncludePath(path.c_str());

  Plugins::Message::get_mutable_instance().show(out.str());
}

void
Document::initialize()
{
  boost::mutex::scoped_lock lock(mutex_);

  if (wasInitialized_)
  {
    return;
  }

  std::for_each(includes_.begin(),
      includes_.end(),
      boost::bind(&Document::addIncludePath,
          boost::ref(*this),
          _1));

  std::for_each(sysIncludes_.begin(),
      sysIncludes_.end(),
      boost::bind(&Document::addSysIncludePath,
          boost::ref(*this),
          _1));

  Tokens::TokenSequence& collection = collection_;
  Tokens::TokenSequence tokens = Tokens::getEachTokenFromFile(fileName_);

  std::copy(tokens.begin(), tokens.end(), std::back_inserter(collection_));

  parse();
  wasInitialized_ = true;
}

bool
Document::addIncludePath(const std::string& path)
{
  std::stringstream out;
  bool response = paths_.add_include_path(path.c_str(), false);

  out << "add include path: " <<path<< " "<< response <<std::endl;

  Plugins::Message::get_mutable_instance().show(out.str());

  return response;
}

bool
Document::addSysIncludePath(const std::string& path)
{
  std::stringstream out;
  bool response = paths_.add_include_path(path.c_str(), true);

  out << "add sysInclude path: " <<path<< " "<< response <<std::endl;

  Plugins::Message::get_mutable_instance().show(out.str());

  return response;
}

static std::string toString(const std::string& path)
{
  return path.c_str();
}

static void
addDefine(PrecompilerContext& context, const std::string& macro)
{
  context.add_macro_definition(macro);
}

std::string
removeCommentsOfConfigFile(const std::string& line)
{
  std::string response;

  if (line.empty())
  {
    return response;
  }

  std::string::size_type pos = line.find("#");

  if (pos < line.size())
  {
    response = line.substr(0, pos);
  }
  else
  {
    response = line;
  }

  return response;
}

void addParameterToContext(const std::string& line)
{
  if (line.empty())
  {
    return;
  }

  std::string::size_type pos = line.find("+=");
  if (pos != std::string::npos)
  {
    std::string name = line.substr(0, pos);
    std::string value = line.substr(pos + 2);

    if (name.compare(INCLUDE_LABEL) == 0)
    {
      includes_.push_back(value);
    }
    else if (name.compare(SYSTEM_INCLUDE_LABEL) == 0)
    {
      sysIncludes_.push_back(value);
    }
    else if (name.compare(MACRO_LABEL) == 0)
    {
      preDefines_.push_back(value);
    }
  }
  else
  {
    std::ostringstream ss;
    ss << "Invalid parameter association: " << line;
    throw Vera::Structures::DocumentError(ss.str());
  }
}

void
Document::readConfigFile(std::istream& in)
{
  std::string line;
  int lineNumber = 0;
  while (std::getline(in, line))
  {
    ++lineNumber;

    if (line.empty())
    {
      continue;
    }

    std::string content = removeCommentsOfConfigFile(line);
    addParameterToContext(content);
  }
}

void
Document::readConfigFile(const std::string& fileName)
{
  if (fileName.empty())
  {
    return;
  }

  std::ifstream file(fileName.c_str());
  if (file.is_open() == false)
  {
    std::ostringstream ss;
    ss << "Cannot open config file " << fileName << ": "
        << strerror(errno);
    throw DocumentError(ss.str());
  }

  readConfigFile(file);

  if (file.bad())
  {
    throw std::ios::failure(
        "Cannot read from " + fileName + ": " + strerror(errno));
  }

  file.close();
}

void
Document::parse()
{
  Tokens::TokenSequence::const_iterator it = collection_.begin();
  Tokens::TokenSequence::const_iterator end = collection_.end();

  std::stringstream out;

  out << "Parse: " << fileName_ << std::endl;

  Plugins::Message::get_mutable_instance().show(out.str());

  for (; it < end; ++it)
  {
    if (it->name_ == EOF_TOKEN_NAME)
    {
      continue;
    }

    if (IsValidTokenForStatement()(*it) == true)
    {
      StatementsBuilder partial(root_);
      partial.builder(it, end);

      if (partial.isSignature(root_.getBack()))
      {
        root_.getBack().type_ =
            Statement::TYPE_ITEM_STATEMENT_OF_SIGNATURE_DECLARATION;
      }

      if (it != end && IsValidTokenForStatement()(*it) == false)
      {
        root_.push(*it);
      }
    }
    else
    {
      root_.push(*it);
    }
  }
}

const Statement&
Document::getRoot()
{
  return root_;
}

boost::mutex _mutex_;

boost::shared_ptr<Document>
Document::create(std::string fileName)
{
  boost::mutex::scoped_lock lock(_mutex_);

  DocumentSequence::const_iterator it = documents_.find(fileName);

  if (it != documents_.end())
  {
    return it->second;
  }

  const SourceFiles::FileNameSet& files = SourceFiles::getAllFileNames();

  if (files.find(fileName) !=  files.end())
  {
    boost::shared_ptr<Document> doc = boost::make_shared < Document > (fileName);

    documents_[fileName] = doc;

    return doc;
  }

  return boost::shared_ptr<Document>();
}

void
Document::addDefine(std::string name, std::size_t id)
{
  defineMap_[id] = name;
}
void
Document::addStruct(std::string name, std::size_t id)
{
  structMap_[id] = name;
}

void
Document::addEnum(std::string name, std::size_t id)
{
  enumMap_[id] = name;
}

void
Document::addClass(std::string name, std::size_t id)
{
  classMap_[id] = name;
}

void
Document::addUnion(std::string name, std::size_t id)
{
  unionMap_[id] = name;
}

void
Document::addTypedef(std::string name, std::size_t id)
{
  typedefMap_[id] = name;
}

Document::RegisterItems
Document::getRegisterDefine()
{
  return defineMap_;
}

Document::RegisterItems
Document::getRegisterStruct()
{
  return structMap_;
}

Document::RegisterItems
Document::getRegisterEnum()
{
  return enumMap_;
}

Document::RegisterItems
Document::getRegisterClass()
{
  return classMap_;
}

Document::RegisterItems
Document::getRegisterTypedef()
{
  return typedefMap_;
}

void
Document::parseHeader(const std::string& content)
{
  std::string file = content;
  boost::filesystem::path path = paths_.get_current_directory();
  std::string dir = "";

  bool response = paths_.find_include_file(file, dir, false, NULL);

  if (response == true)
  {
    std::stringstream out;

    out << "header file: " << file << "  "<<content << content<< std::endl;

    Plugins::Message::get_mutable_instance().show(out.str());

    boost::shared_ptr<Document> doc = Document::create(file);

    if (doc)
    {
      doc->initialize();
    }
  }
}

void
Document::enableUnion()
{
  isUnion_ = true;
}

void
Document::disableUnion()
{
  isUnion_ = false;
}

void
Document::enableFunction()
{
  isFunction_ = true;
}

void
Document::disableFunction()
{
  isFunction_ = false;
}

bool
Document::isFunction()
{
  return isFunction_;
}

bool
Document::isUnion()
{
  return isUnion_;
}

} // Vera namespace
} // Structures namespace
