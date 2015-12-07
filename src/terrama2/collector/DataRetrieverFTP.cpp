/*
  Copyright (C) 2007 National Institute For Space Research (INPE) - Brazil.

  This file is part of TerraMA2 - a free and open source computational
  platform for analysis, monitoring, and alert of geo-environmental extremes.

  TerraMA2 is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  TerraMA2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with TerraMA2. See LICENSE. If not, write to
  TerraMA2 Team at <terrama2-team@dpi.inpe.br>.
*/

/*!
  \file terrama2/collector/DataRetrieverFTP.cpp

  \brief .

  \author Jano Simas
  \author Evandro Delatin
*/

// STL
#include <iostream>
#include <fstream>
#include <vector>

// TerraMA2
#include "DataRetrieverFTP.hpp"
#include "DataFilter.hpp"
#include "Exception.hpp"
#include "Log.hpp"

// Libcurl
#include <curl/curl.h>

// Boost
#include <boost/algorithm/string.hpp>

// QT
#include <QTranslator>

terrama2::collector::DataRetrieverFTP::DataRetrieverFTP(const terrama2::core::DataProvider& dataprovider)
  : DataRetriever(dataprovider)
{

}

terrama2::collector::DataRetrieverFTP::~DataRetrieverFTP()
{
  try
  {
   std::string path;
// Remove the files in the tmp folder
    for(std::string file: vectorNames_)
    {
      path = "/tmp/"+file;
      std::remove(path.c_str()); // delete file
    }
  }
  catch(const std::exception& e)
  {
    QString messageError = QObject::tr("Could not deleted file! \n\n Details: \n");
    messageError.append(e.what());

    throw DataRetrieverFTPError() << ErrorDescription(messageError);
  }

  catch(...)
  {
    throw DataRetrieverFTPError() << ErrorDescription(QObject::tr("Unknown Error, could not deleted file!"));
  }
}


void terrama2::collector::DataRetrieverFTP::open()
{

  curl_ = curl_easy_init();
}

bool terrama2::collector::DataRetrieverFTP::isOpen()
{
// check if connection is open

  CURLcode status;
  if(curl_)
  {
    curl_easy_setopt(curl_, CURLOPT_URL, dataprovider_.uri().c_str());
    curl_easy_setopt(curl_, CURLOPT_FTPLISTONLY, 1);
    curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT, 3);
    curl_easy_setopt(curl_, CURLOPT_NOBODY, 1);

    status = curl_easy_perform(curl_);

    if (status != CURLE_OK)
      return false;
  }

  curl_easy_cleanup(curl_);
  curl_ = curl_easy_init();

  return true;

}

void terrama2::collector::DataRetrieverFTP::close()
{
// always cleanup
  curl_easy_cleanup(curl_);
}

static size_t write_response(void *ptr, size_t size, size_t nmemb, void *data)
{
  FILE *writehere = (FILE *)data;
  return fwrite(ptr, size, nmemb, writehere);
}


size_t write_vector(void *ptr, size_t size, size_t nmemb, void *data)
{
  size_t sizeRead = size * nmemb;

  std::string* block = (std::string*) data;
  block->append((char *)ptr, sizeRead);

  return sizeRead;
}


std::string terrama2::collector::DataRetrieverFTP::retrieveData(terrama2::core::DataSetItem datasetitem, DataFilterPtr filter, std::vector<std::string>& log_uris)
{
  std::string uriOutput;
  std::string uriInput;
  CURLcode status;
  std::vector<std::string> vectorFiles;
  std::string block;

  try
  {
    if(curl_)
    {
// Get a file listing from server
      uriInput = dataprovider_.uri() + datasetitem.path();
      curl_easy_setopt(curl_, CURLOPT_URL, uriInput.c_str());
      curl_easy_setopt(curl_, CURLOPT_DIRLISTONLY, 1);
      curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, write_vector);
      curl_easy_setopt(curl_, CURLOPT_WRITEDATA, (void *)&block);

// performs the configurations of curl_easy_setop
      status = curl_easy_perform(curl_);

      if (status == CURLE_OK)
      {
        boost::split(vectorFiles, block, boost::is_any_of("\n"));

        if(vectorFiles.size() && vectorFiles.back().empty())
          vectorFiles.pop_back();
      }
      else
      {
        QString messageError = QObject::tr("Could not list the FTP server files. \n\n Details: \n");
        messageError.append(curl_easy_strerror(status));
        throw DataRetrieverFTPError() << ErrorDescription(messageError);
      }

// filter file names that should be downloaded.
      vectorNames_.clear();

      vectorNames_ = filter->filterNames(vectorFiles);

      for (std::string file: vectorNames_)
      {
        CURL *curl;
        FILE *destFilePath;
        CURLcode res;

        curl = curl_easy_init();

        if (curl)
        {
          uriOutput = dataprovider_.uri() + datasetitem.path() + file;
          destFilePath = fopen(("/tmp/"+file).c_str(),"wb");
          curl_easy_setopt(curl, CURLOPT_URL, uriOutput.c_str());
          curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
          curl_easy_setopt(curl, CURLOPT_WRITEDATA, destFilePath);
          res = curl_easy_perform(curl);

          if (res != CURLE_OK)
          {
            QString messageError = QObject::tr("Could not perform the download files. \n\n Details: \n");
            messageError.append(curl_easy_strerror(res));
            throw DataRetrieverFTPError() << ErrorDescription(messageError);
          }
          else
          {
            curl_easy_cleanup(curl);

            fclose(destFilePath);

            log_uris.push_back(uriInput);
          }
        }
      }
    }

  }
  catch(const std::exception& e)
  {
    QString messageError = QObject::tr("Could not perform the download files! \n\n Details: \n");
    messageError.append(e.what());

    throw DataRetrieverFTPError() << ErrorDescription(messageError);
  }

  catch(...)
  {
    throw DataRetrieverFTPError() << ErrorDescription(QObject::tr("Unknown Error, Could not perform the download files!"));
  }

  // returns the absolute path of the folder that contains the files that have been made the download.
  return "file:///tmp/";
}