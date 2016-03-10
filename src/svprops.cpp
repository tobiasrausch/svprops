/*
============================================================================
SV VCF properties
============================================================================
Copyright (C) 2015 Tobias Rausch

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
============================================================================
Contact: Tobias Rausch (rausch@embl.de)
============================================================================
*/

#include <iostream>
#include <set>
#include <vector>
#include <algorithm>
#include <map>
#include "htslib/vcf.h"


template<typename TVector>
inline void
_getMedian(TVector& v, typename TVector::value_type& med) {
  med = 0;
  if (v.size()) {
    typename TVector::iterator begin = v.begin();
    typename TVector::iterator end = v.end();
    std::nth_element(begin, begin + (end - begin) / 2, end);
    med = *(begin + (end - begin) / 2);
  }
}

inline bool
_isKeyPresent(bcf_hdr_t const* hdr, std::string const& key) {
  return (bcf_hdr_id2int(hdr, BCF_DT_ID, key.c_str())>=0);
}

inline int
_getInfoType(bcf_hdr_t const* hdr, std::string const& key) {
  return bcf_hdr_id2type(hdr, BCF_HL_INFO, bcf_hdr_id2int(hdr, BCF_DT_ID, key.c_str()));
}

inline int
_getFormatType(bcf_hdr_t const* hdr, std::string const& key) {
  return bcf_hdr_id2type(hdr, BCF_HL_FMT, bcf_hdr_id2int(hdr, BCF_DT_ID, key.c_str()));
}

inline bool _missing(bool const value) {
  return !value;
}

inline bool _missing(float const value) {
  return bcf_float_is_missing(value);
}

inline bool _missing(int8_t const value) {
  return (value == bcf_int8_missing);
}

inline bool _missing(int16_t const value) {
  return (value == bcf_int16_missing);
}

inline bool _missing(int32_t const value) {
  return (value == bcf_int32_missing);
}

inline bool _missing(std::string const& value) {
  return ((value.empty()) || (value == "."));
}


int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <in.vcf.gz> " << std::endl;
    return 1; 
  }

  // Load bcf file
  htsFile* ifile = bcf_open(argv[1], "r");
  if (ifile == NULL) {
    std::cerr << "Fail to load " << argv[1] << std::endl;
    return 1;
  }
  bcf_hdr_t* hdr = bcf_hdr_read(ifile);

  // Read SV information
  int32_t nsvend = 0;
  int32_t* svend = NULL;
  int32_t ninslen = 0;
  int32_t* inslen = NULL;
  int32_t ncipos = 0;
  int32_t* cipos = NULL;
  int32_t nsvt = 0;
  char* svt = NULL;
  int32_t nfic = 0;
  float* fic = NULL;
  int32_t nrsq = 0;
  float* rsq = NULL;
  int32_t nhwepval = 0;
  float* hwepval = NULL;
  int ngt = 0;
  int32_t* gt = NULL;
  int nrc = 0;
  int32_t* rc = NULL;
  int nrcl = 0;
  int32_t* rcl = NULL;
  int nrcr = 0;
  int32_t* rcr = NULL;
  int ndv = 0;
  int32_t* dv = NULL;
  int ndr = 0;
  int32_t* dr = NULL;
  int nrv = 0;
  int32_t* rv = NULL;
  int nrr = 0;
  int32_t* rr = NULL;
  int ngq = 0;
  int32_t* gqInt = NULL;
  float* gqFloat = NULL;

  // Get the valid columns
  uint32_t fieldIndex = 0;
  typedef std::map<std::string, uint32_t> TColumnMap;
  TColumnMap cMap;
  cMap["chr"] = fieldIndex++;
  cMap["start"] = fieldIndex++;
  cMap["end"] = fieldIndex++;
  cMap["id"] = fieldIndex++;
  cMap["size"] = fieldIndex++;
  cMap["vac"] = fieldIndex++;
  cMap["vaf"] = fieldIndex++;
  cMap["singleton"] = fieldIndex++;
  cMap["missingrate"] = fieldIndex++;
  if (_isKeyPresent(hdr, "SVTYPE")) cMap["svtype"] = fieldIndex++;
  if (_isKeyPresent(hdr, "IMPRECISE")) cMap["precise"] = fieldIndex++;
  if (_isKeyPresent(hdr, "CIPOS")) cMap["ci"] = fieldIndex++;
  if (_isKeyPresent(hdr, "FIC")) cMap["fic"] = fieldIndex++;
  if (_isKeyPresent(hdr, "RSQ")) cMap["rsq"] = fieldIndex++;
  if (_isKeyPresent(hdr, "HWEpval")) cMap["hwepval"] = fieldIndex++;
  if (_isKeyPresent(hdr, "GQ")) {
    cMap["refgq"] = fieldIndex++;
    cMap["altgq"] = fieldIndex++;
  }
  if (_isKeyPresent(hdr, "RC")) {
    cMap["rdratio"] = fieldIndex++;
    cMap["medianrc"] = fieldIndex++;
  }
  if (_isKeyPresent(hdr, "DV")) {
    cMap["refratio"] = fieldIndex++;
    cMap["altratio"] = fieldIndex++;
  }

  typedef std::vector<std::string> TColumnHeader;
  TColumnHeader cHeader(cMap.size());
  for(TColumnMap::const_iterator cIt = cMap.begin(); cIt != cMap.end(); ++cIt) cHeader[cIt->second] = cIt->first;

  // Write header
  for(TColumnHeader::const_iterator cHead = cHeader.begin(); cHead != cHeader.end(); ++cHead) {
    if (cHead != cHeader.begin()) std::cout << "\t";
    std::cout << *cHead;
  }
  std::cout << std::endl;

  // Parse VCF records
  bcf1_t* rec = bcf_init();
  uint32_t siteCount = 0;
  while (bcf_read(ifile, hdr, rec) == 0) {
    ++siteCount;
    bcf_unpack(rec, BCF_UN_ALL);
    bcf_get_format_int32(hdr, rec, "GT", &gt, &ngt);
    if (_isKeyPresent(hdr, "END")) bcf_get_info_int32(hdr, rec, "END", &svend, &nsvend);
    if (_isKeyPresent(hdr, "INSLEN")) bcf_get_info_int32(hdr, rec, "INSLEN", &inslen, &ninslen);
    if (_isKeyPresent(hdr, "CIPOS")) bcf_get_info_int32(hdr, rec, "CIPOS", &cipos, &ncipos);
    if (_isKeyPresent(hdr, "FIC")) bcf_get_info_float(hdr, rec, "FIC", &fic, &nfic);
    if (_isKeyPresent(hdr, "RSQ")) bcf_get_info_float(hdr, rec, "RSQ", &rsq, &nrsq);
    if (_isKeyPresent(hdr, "HWEpval")) bcf_get_info_float(hdr, rec, "HWEpval", &hwepval, &nhwepval);
    if (_isKeyPresent(hdr, "SVTYPE")) bcf_get_info_string(hdr, rec, "SVTYPE", &svt, &nsvt);
    if (_isKeyPresent(hdr, "GQ")) {
      if (_getFormatType(hdr, "GQ") == BCF_HT_INT) bcf_get_format_int32(hdr, rec, "GQ", &gqInt, &ngq);
      else if (_getFormatType(hdr, "GQ") == BCF_HT_REAL) bcf_get_format_float(hdr, rec, "GQ", &gqFloat, &ngq);
    }
    bool precise = false;
    if (bcf_get_info_flag(hdr, rec, "PRECISE", 0, 0) > 0) precise = true;
    if (_isKeyPresent(hdr, "RC")) bcf_get_format_int32(hdr, rec, "RC", &rc, &nrc);
    if (_isKeyPresent(hdr, "RCL")) bcf_get_format_int32(hdr, rec, "RCL", &rcl, &nrcl);
    if (_isKeyPresent(hdr, "RCR")) bcf_get_format_int32(hdr, rec, "RCR", &rcr, &nrcr);
    if (_isKeyPresent(hdr, "DV")) bcf_get_format_int32(hdr, rec, "DV", &dv, &ndv);
    if (_isKeyPresent(hdr, "DR")) bcf_get_format_int32(hdr, rec, "DR", &dr, &ndr);
    if (_isKeyPresent(hdr, "RV")) bcf_get_format_int32(hdr, rec, "RV", &rv, &nrv);
    if (_isKeyPresent(hdr, "RR")) bcf_get_format_int32(hdr, rec, "RR", &rr, &nrr);

    std::string rareCarrier;
    typedef double TPrecision;
    typedef std::vector<TPrecision> TValueVector;
    TValueVector gqRef;   // GQ of non-carriers
    TValueVector gqAlt;   // GQ of het. carriers
    TValueVector ratioRef;  // SV support in non-carriers
    TValueVector ratioAlt;  // SV support in het. carriers
    TValueVector rcRefRatio;  // Read-depth ratio in non-carriers
    TValueVector rcAltRatio;  // Read-depth ratio in het. carriers
    TValueVector rcRef;  // Baseline read count in non-carriers
    int32_t ac[2];
    ac[0] = 0;
    ac[1] = 0;
    int32_t uncalled = 0;
    for (int i = 0; i < bcf_hdr_nsamples(hdr); ++i) {
      if ((bcf_gt_allele(gt[i*2]) != -1) && (bcf_gt_allele(gt[i*2 + 1]) != -1)) {
        int gt_type = bcf_gt_allele(gt[i*2]) + bcf_gt_allele(gt[i*2 + 1]);
	++ac[bcf_gt_allele(gt[i*2])];
	++ac[bcf_gt_allele(gt[i*2 + 1])];
	if (gt_type == 0) {
	  // Non-carrier
	  if (_isKeyPresent(hdr, "GQ")) {
	    if (_getFormatType(hdr, "GQ") == BCF_HT_INT) {
	      if (_missing(gqInt[i])) gqRef.push_back(0);
	      else gqRef.push_back( gqInt[i] );
	    } else if (_getFormatType(hdr, "GQ") == BCF_HT_REAL) {
	      if (_missing(gqFloat[i])) gqRef.push_back(0);
	      else gqRef.push_back( gqFloat[i] );
	    }
	  }
	  if (_isKeyPresent(hdr, "RC")) {
	    rcRef.push_back( rc[i] );
	    rcRefRatio.push_back( (double) rc[i] / (double) (rcl[i] + rcr[i]) );
	  }
	  if (_isKeyPresent(hdr, "DV")) {
	    if (precise) ratioRef.push_back( (double) rv[i] / (double) (rr[i] + rv[i]) );
	    else ratioRef.push_back( (double) dv[i] / (double) (dr[i] + dv[i]) );
	  }
	} else {
	  // Only het. carrier
	  if (gt_type == 1) {
	    if (ac[1] == 1) rareCarrier = hdr->samples[i];
	    if (_isKeyPresent(hdr, "GQ")) {
	      if (_getFormatType(hdr, "GQ") == BCF_HT_INT) {
		if (_missing(gqInt[i])) gqAlt.push_back(0);
		else gqAlt.push_back( gqInt[i] );
	      } else if (_getFormatType(hdr, "GQ") == BCF_HT_REAL) {
		if (_missing(gqFloat[i])) gqAlt.push_back(0);
		else gqAlt.push_back( gqFloat[i] );
	      }
	    }
	    if (_isKeyPresent(hdr, "RC")) rcAltRatio.push_back( (double) rc[i] / (double) (rcl[i] + rcr[i]) );
	    if (_isKeyPresent(hdr, "DV")) {
	      if (precise) ratioAlt.push_back( (double) rv[i] / (double) (rr[i] + rv[i]) );
	      else ratioAlt.push_back( (double) dv[i] / (double) (dr[i] + dv[i]) );
	    }
	  }
	}
      } else ++uncalled;
    }
    if (ac[1] != 1) rareCarrier = "NA";
    TPrecision af = (TPrecision) ac[1] / (TPrecision) (ac[0] + ac[1]);
    int32_t svlen = 1;
    if (svend != NULL) svlen = *svend - rec->pos;
    if ((svt != NULL) && (std::string(svt) == "INS")) svlen = *inslen; 
    TPrecision missingRate = (TPrecision) uncalled / (TPrecision) bcf_hdr_nsamples(hdr);
    
    TPrecision refratio = 0;
    _getMedian(ratioRef, refratio);
    TPrecision altratio = 0;
    _getMedian(ratioAlt, altratio);
    TPrecision refgq = 0;
    _getMedian(gqRef, refgq);
    TPrecision altgq = 0;
    _getMedian(gqAlt, altgq);
    TPrecision altRC = 0;
    _getMedian(rcAltRatio, altRC);
    TPrecision refRC = 0;
    _getMedian(rcRefRatio, refRC);
    TPrecision rdRatio = altRC / refRC;
    TPrecision rcMed = 0;
    _getMedian(rcRef, rcMed);

    // Write record
    for(TColumnHeader::const_iterator cHead = cHeader.begin(); cHead != cHeader.end(); ++cHead) {
      if (cHead != cHeader.begin()) std::cout << "\t";
      if (*cHead == "chr") std::cout << bcf_hdr_id2name(hdr, rec->rid);
      else if (*cHead == "start") std::cout << (rec->pos + 1);
      else if (*cHead == "end") {
	if (_isKeyPresent(hdr, "END")) std::cout << *svend;
	else std::cout << rec->pos + 1;
      }
      else if (*cHead == "id") std::cout << rec->d.id;
      else if (*cHead == "size") std::cout << svlen;
      else if (*cHead == "vac") std::cout << ac[1];
      else if (*cHead == "vaf") std::cout << af;
      else if (*cHead == "singleton") std::cout << rareCarrier;
      else if (*cHead == "missingrate") std::cout << missingRate;
      else if (*cHead == "svtype") std::cout << svt;
      else if (*cHead == "precise") std::cout << precise;
      else if (*cHead == "ci") std::cout << cipos[1];
      else if (*cHead == "refratio") std::cout << refratio;
      else if (*cHead == "altratio") std::cout << altratio;
      else if (*cHead == "refgq") std::cout << refgq;
      else if (*cHead == "altgq") std::cout << altgq;
      else if (*cHead == "rdratio") std::cout << rdRatio;
      else if (*cHead == "medianrc") std::cout << rcMed;
      else if (*cHead == "fic") std::cout << *fic;
      else if (*cHead == "rsq") std::cout << *rsq;
      else if (*cHead == "hwepval") std::cout << *hwepval;
    }
    std::cout << std::endl;
  }

  // Clean-up
  if (svend != NULL) free(svend);
  if (inslen != NULL) free(inslen);
  if (cipos != NULL) free(cipos);
  if (svt != NULL) free(svt);
  if (fic != NULL) free(fic);
  if (rsq != NULL) free(rsq);
  if (hwepval != NULL) free(hwepval);
  if (gt != NULL) free(gt);
  if (rc != NULL) free(rc);
  if (rcl != NULL) free(rcl);
  if (rcr != NULL) free(rcr);
  if (dv != NULL) free(dv);
  if (dr != NULL) free(dr);
  if (rv != NULL) free(rv);
  if (rr != NULL) free(rr);
  if (gqInt != NULL) free(gqInt);
  if (gqFloat != NULL) free(gqFloat);
  bcf_hdr_destroy(hdr);
  bcf_close(ifile);
  bcf_destroy(rec);

  return 0;
}
