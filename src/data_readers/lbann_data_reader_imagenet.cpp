////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2014-2016, Lawrence Livermore National Security, LLC. 
// Produced at the Lawrence Livermore National Laboratory. 
// Written by the LBANN Research Team (B. Van Essen, et al.) listed in
// the CONTRIBUTORS file. <lbann-dev@llnl.gov>
//
// LLNL-CODE-697807.
// All rights reserved.
//
// This file is part of LBANN: Livermore Big Artificial Neural Network
// Toolkit. For details, see http://software.llnl.gov/LBANN or
// https://github.com/LLNL/LBANN. 
//
// Licensed under the Apache License, Version 2.0 (the "Licensee"); you
// may not use this file except in compliance with the License.  You may
// obtain a copy of the License at:
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the license.
//
// lbann_data_reader_imagenet .hpp .cpp - DataReader class for ImageNet dataset
////////////////////////////////////////////////////////////////////////////////

#include "lbann/data_readers/lbann_data_reader_imagenet.hpp"
#include "lbann/data_readers/lbann_image_utils.hpp"

using namespace std;
using namespace El;


lbann::DataReader_ImageNet::DataReader_ImageNet(int batchSize, bool shuffle)
  : DataReader(batchSize, shuffle)
{
  m_image_width = 256;
  m_image_height = 256;
  m_num_labels = 1000;

  m_pixels = new unsigned char[m_image_width * m_image_height * 3];
}

lbann::DataReader_ImageNet::DataReader_ImageNet(int batchSize)
  : DataReader_ImageNet(batchSize, true) {}

lbann::DataReader_ImageNet::DataReader_ImageNet(const DataReader_ImageNet& source)
  : DataReader((const DataReader&) source),
    m_image_dir(source.m_image_dir), ImageList(source.ImageList),
    m_image_width(source.m_image_width), m_image_height(source.m_image_height), m_num_labels(source.m_num_labels)
{
  m_pixels = new unsigned char[m_image_width * m_image_height * 3];
  memcpy(this->m_pixels, source.m_pixels, m_image_width * m_image_height * 3);
}

lbann::DataReader_ImageNet::~DataReader_ImageNet()
{
  delete m_pixels;
}

int lbann::DataReader_ImageNet::fetch_data(Mat& X)
{
  if(!DataReader::position_valid()) {
    return 0;
  }
	
	int pixelcount = m_image_width * m_image_height;
  int current_batch_size = getBatchSize();
	
  int n = 0;
  for (n = CurrentPos; n < CurrentPos + current_batch_size; n++) {
    if (n >= (int)ShuffledIndices.size())
      break;
			
    int k = n - CurrentPos;
    int index = ShuffledIndices[n];
    string imagepath = m_image_dir + ImageList[index].first;

    int width, height;
    bool ret = lbann::image_utils::loadJPG(imagepath.c_str(), width, height, true, m_pixels);
    if(!ret) {
      throw lbann_exception("ImageNet: image_utils::loadJPG fauled to load");
    }
    if(width != m_image_width || height != m_image_height) {
      throw lbann_exception("ImageNet: mismatch data size -- either width or height");
    }
			
    for (int p = 0; p < pixelcount; p++) {
      X.Set(p, k, m_pixels[p] / 255.0f);
    }
  }

	return (n - CurrentPos);
}

int lbann::DataReader_ImageNet::fetch_label(Mat& Y)
{
  if(!position_valid()) {
    return 0;
  }

  int current_batch_size = getBatchSize();
  int n = 0;
  for (n = CurrentPos; n < CurrentPos + current_batch_size; n++) {
    if (n >= (int)ShuffledIndices.size())
      break;
			
    int k = n - CurrentPos;
    int index = ShuffledIndices[n];
    int label = ImageList[index].second;

    Y.Set(label, k, 1);
  }
	return (n - CurrentPos);
}

bool lbann::DataReader_ImageNet::load(string imageDir, string imageListFile)
{
	m_image_dir = imageDir; /// Store the primary path to the images for use on fetch
	ImageList.clear();

	// load image list
	FILE* fplist = fopen(imageListFile.c_str(), "rt");
	if (!fplist)
		return false;
	while (!feof(fplist)) {
		char imagepath[512];
		int imagelabel;
		if (fscanf(fplist, "%s%d", imagepath, &imagelabel) <= 1)
			break;
		ImageList.push_back(make_pair(imagepath, imagelabel));
	}
	fclose(fplist);
	
	// reset indices
	ShuffledIndices.clear();
	ShuffledIndices.resize(ImageList.size());
	for (size_t n = 0; n < ImageList.size(); n++) {
		ShuffledIndices[n] = n;
	}

	return true;
}

bool lbann::DataReader_ImageNet::load(string imageDir, string imageListFile, size_t max_sample_count, bool firstN) {
  bool load_successful = false;

  load_successful = load(imageDir, imageListFile);

  if(max_sample_count > getNumData() || ((long) max_sample_count) < 0) {
    throw lbann_exception("ImageNet: data reader load error: invalid number of samples selected");
  }
  select_subset_of_data(max_sample_count, firstN);

  return load_successful;
}

bool lbann::DataReader_ImageNet::load(string imageDir, string imageListFile, double use_percentage, bool firstN) {
  bool load_successful = false;

  load_successful = load(imageDir, imageListFile);

  size_t max_sample_count = rint(getNumData()*use_percentage);

  if(max_sample_count > getNumData() || ((long) max_sample_count) < 0) {
    throw lbann_exception("ImageNet: data reader load error: invalid number of samples selected");
  }
  select_subset_of_data(max_sample_count, firstN);

  return load_successful;
}

void lbann::DataReader_ImageNet::free()
{
  delete m_pixels;
}

// Assignment operator
lbann::DataReader_ImageNet& lbann::DataReader_ImageNet::operator=(const DataReader_ImageNet& source)
{
  // check for self-assignment
  if (this == &source)
    return *this;

  // Call the parent operator= function
  DataReader::operator=(source);

  // first we need to deallocate any data that this data reader is holding!
  delete m_pixels;

  this->m_image_dir = source.m_image_dir;
  this->ImageList = source.ImageList;
  this->m_image_width = source.m_image_width;
  this->m_image_height = source.m_image_height;
  this->m_num_labels = source.m_num_labels;

  m_pixels = new unsigned char[m_image_width * m_image_height * 3];
  memcpy(this->m_pixels, source.m_pixels, m_image_width * m_image_height * 3);

  return *this;
}
