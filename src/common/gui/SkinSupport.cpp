/*
** Various support of skin operations for the rudimentary skinning engine
*/

#include "SkinSupport.h"
#include "SurgeStorage.h"
#include "SurgeBitmaps.h"

#if LINUX
#include <experimental/filesystem>
#elif MAC || (WINDOWS && TARGET_RACK)
#include <filesystem.h>
#else
#include <filesystem>
#endif

#include <iostream>

#if !WINDOWS
namespace fs = std::experimental::filesystem;
#endif

namespace Surge
{
namespace UI
{
SkinDB& Surge::UI::SkinDB::get(SurgeStorage* s)
{
   static SkinDB instance(s);
   return instance;
}

SkinDB::SkinDB(SurgeStorage* s)
{
   std::cout << "Constructing SkinDB" << std::endl;
   rescanForSkins(s);
}

SkinDB::~SkinDB()
{
   skins.clear(); // Not really necessary but means the skins are destroyed before the rest of the
                  // dtor runs
   std::cout << "Destroying SkinDB" << std::endl;
}

std::shared_ptr<Skin> SkinDB::defaultSkin()
{
   // for now
   return getSkin(defaultSkinEntry);
}

std::shared_ptr<Skin> SkinDB::getSkin(const Entry& skinEntry)
{
   if (skins.find(skinEntry) == skins.end())
   {
      skins[skinEntry] = std::make_shared<Skin>(skinEntry.root, skinEntry.name);
   }
   return skins[skinEntry];
}

void SkinDB::rescanForSkins(SurgeStorage* storage)
{
   availableSkins.clear();

   std::vector<std::string> paths = {storage->datapath, storage->userDataPath};

   for (auto sourceS : paths)
   {
      fs::path source(sourceS);
      std::vector<fs::path> candidates;

      std::vector<fs::path> alldirs;
      std::deque<fs::path> workStack;
      workStack.push_back(source);
      while (!workStack.empty())
      {
         auto top = workStack.front();
         workStack.pop_front();
         for (auto& d : fs::directory_iterator(top))
         {
            if (fs::is_directory(d))
            {
               alldirs.push_back(d);
               workStack.push_back(d);
            }
         }
      }

      for (auto& p : alldirs)
      {
         std::string name;
#if WINDOWS && !TARGET_RACK
         /*
         ** Windows filesystem names are properly wstrings which, if we want them to
         ** display properly in vstgui, need to be converted to UTF8 using the
         ** windows widechar API. Linux and Mac do not require this.
         */
         std::wstring str = p.wstring();
         name = Surge::Storage::wstringToUTF8(str);
#else
         name = p.generic_string();
#endif

         std::string ending = ".surge-skin";
         if (name.length() >= ending.length() &&
             0 == name.compare(name.length() - ending.length(), ending.length(), ending))
         {
#if WINDOWS
            char sep = '\\';
#else
            char sep = '/';
#endif
            auto sp = name.rfind(sep);
            if (sp == std::string::npos)
            {
            }
            else
            {
               auto path = name.substr(0, sp + 1);
               auto lo = name.substr(sp + 1);
               Entry e;
               e.root = path;
               e.name = lo + sep;
               if (e.name.find("default.surge-skin") != std::string::npos &&
                   defaultSkinEntry.name == "")
               {
                  defaultSkinEntry = e;
               }
               availableSkins.push_back(e);
            }
         }
      }
   }
}

// see scripts/misc/idmap.pl if you want to regen this
std::unordered_map<std::string, int> createIdNameMap()
{
   std::unordered_map<std::string, int> res;
   res["BG"] = 102;
   res["FADERV_BG"] = 105;
   res["FILTERBUTTONS"] = 108;
   res["OSCSWITCH"] = 110;
   res["FBCONFIG"] = 112;
   res["SCENESWITCH"] = 113;
   res["SCENEMODE"] = 114;
   res["OCTAVES_OSC"] = 117;
   res["OCTAVES"] = 118;
   res["OSCMENU"] = 119;
   res["WAVESHAPER"] = 120;
   res["RELATIVE_TOGGLE"] = 121;
   res["OSCSELECT"] = 122;
   res["POLYMODE"] = 123;
   res["MODSRC_BG"] = 124;
   res["SWITCH_KTRK"] = 125;
   res["SWITCH_RETRIGGER"] = 126;
   res["NUMBERS"] = 127;
   res["MODSRC_SYMBOL"] = 128;
   res["FADERH_LABELS"] = 131;
   res["SWITCH_SOLO"] = 132;
   res["SWITCH_FM"] = 133;
   res["SWITCH_MUTE"] = 134;
   res["CONF"] = 135;
   res["FXCONF_SYMBOLS"] = 136;
   res["FXCONF"] = 137;
   res["SWITCH_TEMPOSYNC"] = 140;
   res["SWITCH_LINK"] = 140;
   res["VFADER_MINI_BG_BLACK"] = 141;
   res["OSCROUTE"] = 143;
   res["FXBYPASS"] = 144;
   res["ENVSHAPE"] = 145;
   res["LFOTRIGGER"] = 146;
   res["BUTTON_STORE"] = 148;
   res["BUTTON_MINUSPLUS"] = 149;
   res["BUTTON_CHECK"] = 150;
   res["FMCONFIG"] = 151;
   res["UNIPOLAR"] = 152;
   res["FADERH_HANDLE"] = 153;
   res["FADERH_BG"] = 154;
   res["FADERV_HANDLE"] = 157;
   res["ABOUT"] = 158;
   res["BUTTON_ABOUT"] = 159;
   res["FILTERSUBTYPE"] = 160;
   res["CHARACTER"] = 161;
   res["ENVMODE"] = 162;
   res["STOREPATCH"] = 163;
   res["BUTTON_MENU"] = 164;
   return res;
}

Skin::Skin(std::string root, std::string name) : root(root), name(name)
{
   std::cout << "Constructing a skin " << _D(root) << _D(name) << std::endl;
   imageIds = createIdNameMap();
}

Skin::~Skin()
{
   std::cout << "Destroying a skin" << std::endl;
}

#if !defined(TINYXML_SAFE_TO_ELEMENT)
#define TINYXML_SAFE_TO_ELEMENT(expr) ((expr) ? (expr)->ToElement() : NULL)
#endif

void Skin::reloadSkin(std::shared_ptr<SurgeBitmaps> bitmapStore)
{
   std::cout << "Reloading skin " << _D(name) << std::endl;
   TiXmlDocument doc;
   // Obviously fix this
   doc.SetTabSize(4);

   if (!doc.LoadFile(resourceName("skin.xml")) || doc.Error())
   {
      FIXMEERROR << "Unable to load skin.xml resource '" << resourceName("skin.xml") << "'"
                 << std::endl;
      FIXMEERROR << "Unable to parse bskin.xml\nError is:\n"
                 << doc.ErrorDesc() << " at row=" << doc.ErrorRow() << " col=" << doc.ErrorCol()
                 << std::endl;
   }

   TiXmlElement* surgeskin = TINYXML_SAFE_TO_ELEMENT(doc.FirstChild("surge-skin"));
   if (!surgeskin)
   {
      FIXMEERROR << "There is no top level suge-skin node in skin.xml" << std::endl;
      return;
   }

   const char* a;
   displayName = name;
   author = "";
   authorURL = "";
   if (a = surgeskin->Attribute("name"))
      displayName = a;
   if (a = surgeskin->Attribute("author"))
      author = a;
   if (a = surgeskin->Attribute("authorURL"))
      authorURL = a;

   auto globalsxml = TINYXML_SAFE_TO_ELEMENT(surgeskin->FirstChild("globals"));
   auto controlsxml = TINYXML_SAFE_TO_ELEMENT(surgeskin->FirstChild("controls"));
   if (!globalsxml)
   {
      FIXMEERROR << "surge-skin contains no globals element" << std::endl;
   }
   if (!controlsxml)
   {
      FIXMEERROR << "surge-skin contains no controls element" << std::endl;
   }

   /*
   ** Parse the globals section
   */
   for (auto gchild = globalsxml->FirstChild(); gchild; gchild = gchild->NextSibling())
   {
      auto lkid = TINYXML_SAFE_TO_ELEMENT(gchild);
      if (lkid)
      {
         std::string key = lkid->Value();
         props_t amap;
         for (auto a = lkid->FirstAttribute(); a; a = a->Next())
            amap[a->Name()] = a->Value();
         globals.push_back(std::make_pair(key, amap));
      }
   }

   /*
   ** Pasrse the controls section
   */
   auto attrint = [](TiXmlElement* e, const char* a) {
      const char* av = e->Attribute(a);
      if (!av)
         return -1;
      return std::atoi(av);
   };

   for (auto gchild = controlsxml->FirstChild(); gchild; gchild = gchild->NextSibling())
   {
      auto lkid = TINYXML_SAFE_TO_ELEMENT(gchild);
      if (!lkid)
         continue;

      if (std::string(lkid->Value()) != "control")
      {
         FIXMEERROR << "INVALID CONTROL" << std::endl;
         continue;
      }

      Skin::Control c;
      c.x = attrint(lkid, "x");
      c.y = attrint(lkid, "y");
      c.w = attrint(lkid, "w");
      c.h = attrint(lkid, "h");
      c.posx = attrint(lkid, "posx");
      c.posy = attrint(lkid, "posy");
      c.posy_offset = attrint(lkid, "posy_offset");

      controls.push_back(c);
   }

   // process the images
   for (auto g : globals)
   {
      if (g.first == "defaultimage")
      {
         auto path = resourceName(g.second["directory"]);
         fs::path source(path);
         for (auto& d : fs::directory_iterator(source))
         {
            auto pos = d.path().generic_string().find("bmp001");
            if (pos != std::string::npos)
            {
               int idx = std::atoi(d.path().generic_string().c_str() + pos + 3);
               bitmapStore->loadBitmapByPathForID(d.path().generic_string(), idx);
            }
         }
      }
   }

   colors.clear();
   for (auto g : globals)
   {
      if (g.first == "image")
      {
         auto p = g.second;
         auto id = p["id"];
         auto res = p["resource"];
         // FIXME = error handling
         if (res.size() > 0)
         {
            if (id.size() > 0)
            {
               bitmapStore->loadBitmapByPathForID(resourceName(res), imageIds[id]);
            }
            else
            {
               bitmapStore->loadBitmapByPath(resourceName(res));
            }
         }
      }
      if (g.first == "color")
      {
         auto p = g.second;
         auto id = p["id"];
         auto val = p["value"];
         auto r = VSTGUI::CColor();
         if (val[0] == '#')
         {
            uint32_t rgb;
            sscanf(val.c_str() + 1, "%x", &rgb);

            int b = rgb % 256;
            rgb = rgb >> 8;

            int g = rgb % 256;
            rgb = rgb >> 8;

            int r = rgb % 256;
            colors[id] = VSTGUI::CColor(r, g, b);
         }
         else
         {
            colors[id] = VSTGUI::CColor(255, 0, 0);
         }
      }
   }
}

bool Skin::hasColor(std::string id)
{
   return colors.find(id) != colors.end();
}
VSTGUI::CColor Skin::getColor(std::string id, const VSTGUI::CColor& def)
{
   if (colors.find(id) != colors.end())
      return colors[id];
   return def;
}
} // namespace UI
} // namespace Surge