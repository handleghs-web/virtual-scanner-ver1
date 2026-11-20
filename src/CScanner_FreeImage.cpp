/***************************************************************************
* Copyright � 2007 TWAIN Working Group:  
*   Adobe Systems Incorporated, AnyDoc Software Inc., Eastman Kodak Company, 
*   Fujitsu Computer Products of America, JFL Peripheral Solutions Inc., 
*   Ricoh Corporation, and Xerox Corporation.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the TWAIN Working Group nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY TWAIN Working Group ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL TWAIN Working Group BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
***************************************************************************/

/**
* @file CScanner_FreeImage.cpp
* Defines a scanner.
* Create a virtual scanner.
* @author TWAIN Working Group
* @date April 2007
*/


#include "CScanner_FreeImage.h"

#include <iostream>
#include <time.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef TWH_CMP_MSC
#include <io.h>
#elif __APPLE__
//#include <io.h>
#else //#ifdef TWH_CMP_MSC
#include <sys/io.h>
#endif //#ifdef TWH_CMP_MSC

#ifdef TWNDS_OS_LINUX
  #define kTWAIN_DS_DIR "/usr/local/lib/twain/sample2"
#endif

#include "DSMInterface.h"

using namespace std;

#ifdef TWNDS_OS_APPLE
  #include "CoreFoundation/CoreFoundation.h"
  #include "CoreFoundation/CFBundle.h"
  #include "CoreFoundation/CFURL.h"
  #include <Carbon/Carbon.h>
#endif

/**
* Environment vars to get the Xfer Count.  Create this enviroment Varable on your system to simulate the 
* number of pages sitting in the scanner waiting to be scanned.
*/
#define kGETENV_XFERCOUNT "CAP_XFERCOUNT"

#include <algorithm>
#include <stdarg.h>
#include <stdio.h>
#include <math.h>

#ifdef TWH_CMP_MSC
extern HINSTANCE   g_hinstance;
#endif

//////////////////////////////////////////////////////////////////////////////
void FreeImageErrorHandler(FREE_IMAGE_FORMAT fif, const char *message)
{
	if (CScanner_FreeImage::ms_blShowConsoleMessages)
	{
		cout << "\n*** ";
		cout << FreeImage_GetFormatFromFIF(fif) << " Format" << endl;
		cout << message << " ***" << endl;
		return;
	}
}



//////////////////////////////////////////////////////////////////////////////
// Folder feed
//////////////////////////////////////////////////////////////////////////////
// Create each directory component without passing a trailing backslash to
// CreateDirectoryA. The old implementation attempted to create paths such as
// "C:\\VirtualScan\\" and silently ignored failures; it also did not
// report whether the final feed folders were actually created.
static bool feedMkdirs(const std::string &path)
{
  if (path.empty()) return false;

  std::string normalized = path;
  for (size_t i = 0; i < normalized.size(); ++i)
  {
    if (normalized[i] == '/') normalized[i] = '\\';
  }
  while (normalized.size() > 3 && normalized[normalized.size() - 1] == '\\')
    normalized.erase(normalized.size() - 1);

  bool ok = true;
  for (size_t i = 3; i <= normalized.size(); ++i)
  {
    if (i == normalized.size() || normalized[i] == '\\')
    {
      std::string part = normalized.substr(0, i);
      if (part.empty()) continue;
      if (!::CreateDirectoryA(part.c_str(), NULL))
      {
        DWORD err = ::GetLastError();
        if (err != ERROR_ALREADY_EXISTS)
          ok = false;
      }
    }
  }
  return ok;
}

static std::string feedDsDir()
{
  char sz[MAX_PATH] = { 0 };
#ifdef TWH_CMP_MSC
  ::GetModuleFileNameA(g_hinstance, sz, MAX_PATH);
#else
  ::GetModuleFileNameA(NULL, sz, MAX_PATH);
#endif
  std::string s(sz);
  size_t p = s.find_last_of("\\/");
  return (p == std::string::npos) ? std::string(".") : s.substr(0, p);
}

void CScanner_FreeImage::loadFeedConfig()
{
  // use root= from feed.ini in the DS folder (default C:\VirtualScan)
  char root[MAX_PATH] = { 0 };
  std::string ini = feedDsDir() + "\\feed.ini";
  ::GetPrivateProfileStringA("feed", "root", "C:\\VirtualScan", root, MAX_PATH, ini.c_str());
  m_feedFullPage = (::GetPrivateProfileIntA("feed", "full_page", 1, ini.c_str()) != 0);
  m_feedImageDpi = (double)::GetPrivateProfileIntA("feed", "image_dpi", 0, ini.c_str());
  m_feedRoot = root;
  while (!m_feedRoot.empty() && (m_feedRoot[m_feedRoot.size() - 1] == '\\' || m_feedRoot[m_feedRoot.size() - 1] == '/'))
  {
    m_feedRoot.erase(m_feedRoot.size() - 1);
  }
  m_feedIn   = m_feedRoot + "\\in";
  m_feedDone = m_feedRoot + "\\done";
  m_feedBad  = m_feedRoot + "\\bad";
  bool inOk = feedMkdirs(m_feedIn);
  bool doneOk = feedMkdirs(m_feedDone);
  bool badOk = feedMkdirs(m_feedBad);
  vlog("feed directory creation: in=%d done=%d bad=%d winerr=%lu", (int)inOk, (int)doneOk, (int)badOk, (unsigned long)::GetLastError());
  vlog("=== virtual scanner start: in=%s full_page=%d image_dpi=%.0f ===", m_feedIn.c_str(), (int)m_feedFullPage, m_feedImageDpi);
}

void CScanner_FreeImage::vlog(const char *fmt, ...) const
{
  if (m_feedRoot.empty()) return;
  std::string path = m_feedRoot + "\\feed.log";
  WIN32_FILE_ATTRIBUTE_DATA fad;
  if (::GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fad) && fad.nFileSizeLow > 1000000)
  {
    ::DeleteFileA(path.c_str());   // restart the log when it exceeds 1MB
  }
  FILE *f = fopen(path.c_str(), "a");
  if (!f) return;
  SYSTEMTIME t;
  ::GetLocalTime(&t);
  fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d  ", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(f, fmt, ap);
  va_end(ap);
  fprintf(f, "\n");
  fclose(f);
}

static bool feedNameLess(const std::string &a, const std::string &b)
{
  return _stricmp(a.c_str(), b.c_str()) < 0;
}

std::vector<std::string> CScanner_FreeImage::listFeedImages() const
{
  std::vector<std::string> out;
  WIN32_FIND_DATAA fd;
  HANDLE h = ::FindFirstFileA((m_feedIn + "\\*").c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) return out;
  do
  {
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
    std::string n(fd.cFileName);
    if (n.empty() || n[0] == '.' || n[0] == '~') continue;
    size_t dot = n.find_last_of('.');
    if (dot == std::string::npos) continue;
    std::string ext = n.substr(dot);
    for (size_t i = 0; i < ext.size(); ++i) ext[i] = (char)tolower((unsigned char)ext[i]);
    if (ext != ".tif" && ext != ".tiff" && ext != ".png" && ext != ".jpg" && ext != ".jpeg" && ext != ".bmp") continue;
    if (m_skip.find(n) != m_skip.end()) continue;
    out.push_back(n);
  } while (::FindNextFileA(h, &fd));
  ::FindClose(h);
  std::sort(out.begin(), out.end(), feedNameLess);
  return out;
}

bool CScanner_FreeImage::moveFeedFile(const std::string &src, const std::string &dir, const std::string &name)
{
  std::string dst = dir + "\\" + name;
  if (::MoveFileExA(src.c_str(), dst.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) return true;
  vlog("move failed (err=%lu): %s", (unsigned long)::GetLastError(), src.c_str());
  return false;
}

bool CScanner_FreeImage::hasMoreImages()
{
  // files that are not real images (wrong signature) go to the bad folder right away,
  // so that they do not make the feeder look loaded
  for(;;)
  {
    std::vector<std::string> files = listFeedImages();
    if(files.empty()) return false;
    const std::string path = m_feedIn + "\\" + files[0];
    if(FIF_UNKNOWN != FreeImage_GetFileType(path.c_str())) return true;
    ::Sleep(200);   // maybe still being written
    if(FIF_UNKNOWN != FreeImage_GetFileType(path.c_str())) return true;
    vlog("not an image: %s -> moved to bad", files[0].c_str());
    if(!moveFeedFile(path, m_feedBad, files[0]))
    {
      m_skip.insert(files[0]);
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
bool CScanner_FreeImage::ms_blShowConsoleMessages = false;
CScanner_FreeImage::CScanner_FreeImage() :
  m_pDIB(0),
  m_nScanLine(0),
  m_bReadOnly(false),
  m_nDestBytesPerRow(0),
  m_nRowOffset(0),
  m_nDocCount(0),
  m_nSourceWidth(0),
  m_nSourceHeight(0)
{
  memset(m_szSourceImagePath, 0, PATH_MAX);

#ifdef TWH_CMP_MSC
  char szEnv[8];
  CScanner_FreeImage::ms_blShowConsoleMessages = (::GetEnvironmentVariableA("TWAIN_SHOWCONSOLEMESSAGES", szEnv, sizeof(szEnv)) != 0);
#else
  CScanner_FreeImage::ms_blShowConsoleMessages = (getenv("TWAIN_SHOWCONSOLEMESSAGES") != 0);
#endif

#ifdef TWH_CMP_MSC
  char szTWAIN_DS_DIR[PATH_MAX];
  GetModuleFileName(g_hinstance, szTWAIN_DS_DIR, PATH_MAX);
  // strip filename from path
  size_t x = strlen(szTWAIN_DS_DIR);
  while(x > 0)
  {
	  if(PATH_SEPERATOR == szTWAIN_DS_DIR[x-1])
	  {
		  szTWAIN_DS_DIR[x-1] = 0;
		  break;
	  }
	  --x;
  }
  SSNPRINTF(m_szSourceImagePath, sizeof(m_szSourceImagePath), PATH_MAX, "%s%cTWAIN_logo.png", szTWAIN_DS_DIR, PATH_SEPERATOR);
#elif defined(TWNDS_OS_APPLE)	
  CFURLRef		fileURL = NULL;
  CFBundleRef		bundle;
		
  // find the sample.pict in our bundle...
  bundle = CFBundleGetBundleWithIdentifier ( CFSTR( "com.yourcompany.TWAINDS-Mac" ) );
  fileURL = CFBundleCopyResourceURL(bundle, CFSTR("TWAIN_logo"), CFSTR("png"), NULL);
  if (fileURL)
  {	
    FSRef 				fsRef;
	if ( CFURLGetFSRef(fileURL, &fsRef) )
	{
	  FSRefMakePath(&fsRef, (UInt8*)m_szSourceImagePath, PATH_MAX);
	}
	CFRelease(fileURL);
  }
#else
  // In Linux, kTWAIN_DS_DIR is set on the compiler command line
  SSNPRINTF(m_szSourceImagePath, sizeof(m_szSourceImagePath), PATH_MAX, "%s%cTWAIN_logo.png", kTWAIN_DS_DIR, PATH_SEPERATOR);
#endif


  // set default caps
  loadFeedConfig();
  resetScanner();

  FreeImage_Initialise();
  FreeImage_SetOutputMessage(FreeImageErrorHandler);
}

//////////////////////////////////////////////////////////////////////////////
CScanner_FreeImage::~CScanner_FreeImage()
{
  if(0 != m_pDIB)
  {
    FreeImage_Unload(m_pDIB);
  }

  FreeImage_DeInitialise();
}


//////////////////////////////////////////////////////////////////////////////
bool CScanner_FreeImage::resetScanner()
{
  bool bret = true;

  // Unlock the scanner 
  Unlock();

  m_nScanLine        = 0;
  m_nDestBytesPerRow = 0;
  m_nRowOffset       = 0;

  m_nDocCount = m_nMaxDocCount = getDocumentCount(); // Reloaded the scanner with paper
  m_nPixelType    = TWPT_RGB;
  m_nPaperSource  = SFI_PAPERSOURCE_ADF;
  m_bDuplex       = false;
  m_nRight        = 0;
  m_nBottom       = 0;
  m_nLeft         = 0;
  m_nTop          = 0;
  m_fXResolution  = 200.0;
  m_fYResolution  = 200.0;

  if(0 != m_pDIB)
  {
    FreeImage_Unload(m_pDIB);
    m_pDIB = 0;
  }

  return bret;
}

//////////////////////////////////////////////////////////////////////////////
SFreeImage CScanner_FreeImage::getSetting() const
{
  return (SFreeImage)(*this);
}

//////////////////////////////////////////////////////////////////////////////
void CScanner_FreeImage::setSetting(SFreeImage settings)
{
  m_nPaperSource = settings.m_nPaperSource;
  m_bDuplex      = settings.m_bDuplex;
  m_nPixelType   = settings.m_nPixelType;
  m_nRight       = settings.m_nRight;
  m_nBottom      = settings.m_nBottom;
  m_nLeft        = settings.m_nLeft;
  m_nTop         = settings.m_nTop;
  m_fXResolution = settings.m_fXResolution;
  m_fYResolution = settings.m_fYResolution;
  m_fGamma       = settings.m_fGamma;
  m_fContrast    = settings.m_fContrast;
  m_fBrightness  = settings.m_fBrightness;
  m_fThreshold   = settings.m_fThreshold;

  if (m_nPaperSource == SFI_PAPERSOURCE_ADF)
  {
	  m_nDocCount = m_nMaxDocCount;
  }
  else
  {
	  m_nDocCount = 1;
  }
}

//////////////////////////////////////////////////////////////////////////////
bool CScanner_FreeImage::acquireImage()
{
  // first, avoid a memory leak by cleaning up past images
  if(0 != m_pDIB)
  {
    FreeImage_Unload(m_pDIB);
    m_pDIB = 0;
  }

  // take the next image from the feed folder (sorted by file name)
  std::vector<std::string> files = listFeedImages();
  if(files.empty())
  {
    vlog("acquire: feed folder is empty");
    return false;
  }
  const std::string name = files[0];
  const std::string path = m_feedIn + "\\" + name;
  strncpy(m_szSourceImagePath, path.c_str(), PATH_MAX - 1);

  FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(path.c_str());
  if(FIF_UNKNOWN == fif)
  {
    fif = FreeImage_GetFIFFromFilename(path.c_str());
  }
  m_pDIB = (FIF_UNKNOWN != fif) ? FreeImage_Load(fif, path.c_str()) : 0;
  if(0 == m_pDIB)
  {
    // the file may still be being written: retry once
    ::Sleep(300);
    m_pDIB = (FIF_UNKNOWN != fif) ? FreeImage_Load(fif, path.c_str()) : 0;
  }

  if(0 == m_pDIB)
  {
    vlog("acquire: could not load %s -> moved to bad", name.c_str());
    if(!moveFeedFile(path, m_feedBad, name))
    {
      m_skip.insert(name);
    }
    return acquireImage();   // continue with the next image
  }

  vlog("acquire: %s (%u x %u, %u bpp, %.0f dpi)", name.c_str(),
       (unsigned)FreeImage_GetWidth(m_pDIB), (unsigned)FreeImage_GetHeight(m_pDIB),
       (unsigned)FreeImage_GetBPP(m_pDIB), FreeImage_GetDotsPerMeterX(m_pDIB) / 39.37);

  // served image: move it to the done folder so that it is not read twice
  if(!moveFeedFile(path, m_feedDone, name))
  {
    m_skip.insert(name);
  }

  //Document scanned, remove it from simulated intray
  m_nDocCount--;

  // do whatever tranforms to the scanned image that was requested by the app
  // before the image is sent to the app.
  if(false == preScanPrep())
  {
    return false;
  }

  return true;
}

//////////////////////////////////////////////////////////////////////////////
bool CScanner_FreeImage::preScanPrep()
{
  // convert all scanned images to 24 bit as a starting point, if they are less
  // then 24 bit.  This is because the scaling functions only work on 8/24/32
  // bit images.
  if(FreeImage_GetBPP(m_pDIB) < 24 )
  {
    FIBITMAP *pDib = FreeImage_ConvertTo24Bits(m_pDIB);
    FreeImage_Unload(m_pDIB);
    m_pDIB = pDib;
  }

  FIBITMAP *pDib = 0;

  if(m_nPixelType != TWPT_BW)
  {
    if(m_fBrightness!=0)
    {
      FreeImage_AdjustBrightness(m_pDIB, m_fBrightness/10);
    }
    if(m_fContrast!=0)
    {
      FreeImage_AdjustContrast(m_pDIB, m_fContrast/10);
    }
    if(m_fGamma!=1.0)
    {
      FreeImage_AdjustGamma(m_pDIB, m_fGamma);
    }
  }

  m_nSourceWidth   = FreeImage_GetWidth(m_pDIB);
  m_nSourceHeight  = FreeImage_GetHeight(m_pDIB);
  // dpi stored in the image (0 = unknown -> assume the requested resolution)
  double imgDpiX = FreeImage_GetDotsPerMeterX(m_pDIB) / 39.37;
  double imgDpiY = FreeImage_GetDotsPerMeterY(m_pDIB) / 39.37;
  if(m_feedImageDpi > 0.0) { imgDpiX = imgDpiY = m_feedImageDpi; }
  if(imgDpiX < 10.0) imgDpiX = m_fXResolution;
  if(imgDpiY < 10.0) imgDpiY = m_fYResolution;
  const double sx = m_fXResolution / imgDpiX;
  const double sy = m_fYResolution / imgDpiY;
  vlog("prep: requested pixeltype=%d dpi=%.0fx%.0f, image dpi=%.0fx%.0f, scale=%.3fx%.3f",
       (int)m_nPixelType, m_fXResolution, m_fYResolution, imgDpiX, imgDpiY, sx, sy);

  // rescale only when the requested dpi really differs (the original sample rescaled
  // with integer division and distorted the image even when the dpi was identical)
  if(fabs(sx - 1.0) > 0.005 || fabs(sy - 1.0) > 0.005)
  {
    WORD unNewWidth  = WORD(m_nSourceWidth  * sx + 0.5);
    WORD unNewHeight = WORD(m_nSourceHeight * sy + 0.5);
    if (CScanner_FreeImage::ms_blShowConsoleMessages)
    {
      cout << "ds: rescaling... to " << unNewWidth << " x " << unNewHeight << endl;
    }
    pDib = FreeImage_Rescale( m_pDIB, unNewWidth, unNewHeight, FILTER_BILINEAR);
    if(0 == pDib)
    {
      return false;
    }
    else
    {
      FreeImage_Unload(m_pDIB);
      m_pDIB = pDib;
    }
  }

  if(m_feedFullPage || m_nRight <= 0 || m_nBottom <= 0)
  {
    m_nRight  = m_nSourceWidth  = FreeImage_GetWidth(m_pDIB);
    m_nBottom = m_nSourceHeight = FreeImage_GetHeight(m_pDIB);
    m_nLeft = 0;
    m_nTop = 0;
  }
  else
  {
    m_nSourceWidth  = FreeImage_GetWidth(m_pDIB);
    m_nSourceHeight = FreeImage_GetHeight(m_pDIB);
  }

  FreeImage_SetDotsPerMeterX( m_pDIB, WORD(m_fXResolution*39.37 + 0.5) );
  FreeImage_SetDotsPerMeterY( m_pDIB, WORD(m_fYResolution*39.37 + 0.5) );
  
  if(m_nPixelType != TWPT_RGB)
  {
    // Apply bit depth color transforms
    switch(m_nPixelType)
    {
      case TWPT_BW:
		if (CScanner_FreeImage::ms_blShowConsoleMessages)
		{
			cout << "ds: converting to TWPT_BW..." << endl;
		}
        /// @todo impliment Dithering
        /// @todo add Threshold setting
        pDib = FreeImage_Threshold(m_pDIB, (BYTE)m_fThreshold);
      break;

      case TWPT_GRAY:
		if (CScanner_FreeImage::ms_blShowConsoleMessages)
		{
			cout << "ds: converting to TWPT_GRAY..." << endl;
		}
        pDib = FreeImage_ConvertTo8Bits(m_pDIB);
      break;

      case TWPT_RGB:
		if (CScanner_FreeImage::ms_blShowConsoleMessages)
		{
			cout << "ds: converting toTWPT_RGB..." << endl;
		}
        pDib = FreeImage_ConvertTo24Bits(m_pDIB);
      break;
    }

    if(0 == pDib)
    {
      return false;
    }
    else
    {
      FreeImage_Unload(m_pDIB);
      m_pDIB = pDib;
    }
  }

  switch(m_nPixelType)
  {
    case TWPT_BW:
      m_nDestBytesPerRow = BYTES_PERLINE(m_nRight-m_nLeft, 1);
      m_nRowOffset = BYTES_PERLINE(m_nLeft, 1);
      break;

    case TWPT_GRAY:
      m_nDestBytesPerRow = BYTES_PERLINE(m_nRight-m_nLeft, 8);
      m_nRowOffset = BYTES_PERLINE(m_nLeft, 8);
      break;

    case TWPT_RGB:
      m_nDestBytesPerRow = BYTES_PERLINE(m_nRight-m_nLeft, 24);
      m_nRowOffset = BYTES_PERLINE(m_nLeft, 24);
      break;
  }

  // setup some convenience vars because they are used during 
  // every strip request
  m_nScanLine       = m_nTop;

  return true;
}

//////////////////////////////////////////////////////////////////////////////
// We want to simulate getting a scan form a scanner.
// if a size larger than the paper is scanned then there will be black on the bottom 
// and to the right of the image.  We want to transfer the image top to bottom, 
// the black will be transfered after the image if neccessary.
bool CScanner_FreeImage::getScanStrip(BYTE *pTransferBuffer, DWORD dwRead, DWORD &dwReceived)
{
  dwReceived = 0;

  if( NULL == pTransferBuffer ||    // Invalid paramiter
      dwRead < m_nDestBytesPerRow ) // Need enough memory to transfer at least an entire row
  {
    return false;
  }
  
  BYTE   *pBits     = NULL;
  WORD    nRow      = 0;
  WORD    nMaxRows  = (WORD)(dwRead / m_nDestBytesPerRow); //number of rows to be transfered during this call (function of buffer size and line size)

  if( m_nScanLine < MIN(m_nSourceHeight, m_nBottom) )
  {
    //fill the buffer line by line to take care of alignment differences
    for(nRow = 0; nRow<nMaxRows; nRow++)
    {
      //get the next scan line position and copy it
      pBits = (BYTE*)FreeImage_GetScanLine(m_pDIB, m_nSourceHeight-m_nScanLine-1);

      memcpy( pTransferBuffer, pBits+m_nRowOffset, MIN(m_nDestBytesPerRow, FreeImage_GetLine(m_pDIB)) );

      // Check to see if the result image width is wider than what we have.
      // If it is wider fill it in with 0es
      if(m_nDestBytesPerRow > FreeImage_GetLine(m_pDIB))
      {
        memset( pTransferBuffer+FreeImage_GetLine(m_pDIB), 0, m_nDestBytesPerRow - FreeImage_GetLine(m_pDIB) );
      }

      //increment the destination by the aligned line size
      pTransferBuffer += m_nDestBytesPerRow;

      // increment the current scanline for next pass
      m_nScanLine++;

      //update the number of bytes written
      dwReceived += m_nDestBytesPerRow;

      // check for finished scan
      if( m_nScanLine >= m_nSourceHeight ||
          m_nScanLine >= m_nBottom )
      {
        //we are done early
        break;
      }
    }
  }

  // Check to see if the result image length is longer than we have.
  // If it is longer fill it in with 0es
  if(m_nBottom > m_nScanLine )
  {
    nMaxRows  = (WORD)((dwRead-dwReceived) / m_nDestBytesPerRow);
    memset( pTransferBuffer, 0, m_nDestBytesPerRow * nMaxRows );
    m_nScanLine += nMaxRows;
    dwReceived += m_nDestBytesPerRow * nMaxRows;
  }

  return true;
}

//////////////////////////////////////////////////////////////////////////////
short CScanner_FreeImage::getDocumentCount() const
{
  // number of images waiting in the feed folder (informational)
  size_t n = listFeedImages().size();
  return (short)(n > 30000 ? 30000 : n);
}

//////////////////////////////////////////////////////////////////////////////
bool CScanner_FreeImage::isFeederLoaded()
{
  // the feeder is loaded as long as the feed folder has images left
  return hasMoreImages();
}

//////////////////////////////////////////////////////////////////////////////
bool CScanner_FreeImage::getDeviceOnline() const
{
  return true;
}

