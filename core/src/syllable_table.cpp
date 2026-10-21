#include "syllable_table.h"

#include <cctype>
#include <sstream>

namespace ninegrid {

namespace {

const char* kSyllableText =
    "a ai an ang ao ba bai ban bang bao bei ben beng bi bian biao bie bin bing bo bu "
    "ca cai can cang cao ce cen ceng cha chai chan chang chao che chen cheng chi chong chou chu "
    "chua chuai chuan chuang chui chun chuo ci cong cou cu cuan cui cun cuo "
    "da dai dan dang dao de dei den deng di dia dian diao die ding diu dong dou du duan dui dun duo "
    "e ei en eng er fa fan fang fei fen feng fo fou fu "
    "ga gai gan gang gao ge gei gen geng gong gou gu gua guai guan guang gui gun guo "
    "ha hai han hang hao he hei hen heng hong hou hu hua huai huan huang hui hun huo "
    "ji jia jian jiang jiao jie jin jing jiong jiu ju juan jue jun "
    "ka kai kan kang kao ke kei ken keng kong kou ku kua kuai kuan kuang kui kun kuo "
    "la lai lan lang lao le lei leng li lia lian liang liao lie lin ling liu long lou lu lv luan lue lun luo "
    "ma mai man mang mao me mei men meng mi mian miao mie min ming miu mo mou mu "
    "na nai nan nang nao ne nei nen neng ni nian niang niao nie nin ning niu nong nou nu nv nuan nuo nve "
    "o ou pa pai pan pang pao pei pen peng pi pian piao pie pin ping po pou pu "
    "qi qia qian qiang qiao qie qin qing qiong qiu qu quan que qun "
    "ran rang rao re ren reng ri rong rou ru ruan rui run ruo "
    "sa sai san sang sao se sen seng sha shai shan shang shao she shei shen sheng shi shou shu "
    "shua shuai shuan shuang shui shun shuo si song sou su suan sui sun suo "
    "ta tai tan tang tao te teng ti tian tiao tie ting tong tou tu tuan tui tun tuo "
    "wa wai wan wang wei wen weng wo wu "
    "xi xia xian xiang xiao xie xin xing xiong xiu xu xuan xue xun "
    "ya yan yang yao ye yi yin ying yo yong you yu yuan yue yun "
    "za zai zan zang zao ze zei zen zeng zha zhai zhan zhang zhao zhe zhei zhen zheng zhi "
    "zhong zhou zhu zhua zhuai zhuan zhuang zhui zhun zhuo zi zong zou zu zuan zui zun zuo";

}  // namespace

char SyllableTable::digit_of(char ch) {
  switch (ch) {
    case 'a': case 'b': case 'c': return '2';
    case 'd': case 'e': case 'f': return '3';
    case 'g': case 'h': case 'i': return '4';
    case 'j': case 'k': case 'l': return '5';
    case 'm': case 'n': case 'o': return '6';
    case 'p': case 'q': case 'r': case 's': return '7';
    case 't': case 'u': case 'v': return '8';
    case 'w': case 'x': case 'y': case 'z': return '9';
    default: return 0;
  }
}

SyllableTable::SyllableTable() {
  weights_ = {
      {"de", 95.0}, {"shi", 90.0}, {"yi", 85.0}, {"wo", 80.0}, {"xian", 80.0},
      {"bu", 70.0}, {"le", 60.0}, {"zai", 60.0}, {"ni", 50.0}, {"zhe", 50.0},
      {"me", 50.0}, {"you", 45.0}, {"hao", 40.0}, {"ma", 30.0}, {"ta", 40.0},
      {"ne", 40.0}, {"wei", 40.0}, {"shen", 40.0}, {"tian", 40.0}, {"zhong", 35.0},
      {"ge", 30.0}, {"a", 30.0}, {"jin", 30.0}, {"ming", 30.0}, {"jian", 25.0},
      {"ban", 25.0}, {"shang", 25.0}, {"xue", 25.0}, {"ba", 25.0}, {"mian", 20.0},
      {"wan", 20.0}, {"lao", 20.0}, {"zen", 20.0}, {"xi", 15.0}, {"an", 15.0},
  };

  std::istringstream stream(kSyllableText);
  std::string syllable;
  while (stream >> syllable) {
    if (index_.find(syllable) != index_.end()) {
      continue;
    }
    std::string digits;
    digits.reserve(syllable.size());
    bool valid = true;
    for (char ch : syllable) {
      const char digit = digit_of(ch);
      if (digit == 0) {
        valid = false;
        break;
      }
      digits.push_back(digit);
    }
    if (!valid) {
      continue;
    }
    index_[syllable] = all_.size();
    all_.push_back(SyllableInfo{syllable, digits, weight(syllable)});
  }
}

const SyllableTable& SyllableTable::instance() {
  static const SyllableTable table;
  return table;
}

bool SyllableTable::is_valid(const std::string& syllable) const {
  return index_.find(syllable) != index_.end();
}

std::string SyllableTable::encode(const std::string& text) const {
  std::string digits;
  digits.reserve(text.size());
  for (char ch : text) {
    const char digit = digit_of(ch);
    if (digit == 0) {
      return std::string();
    }
    digits.push_back(digit);
  }
  return digits;
}

double SyllableTable::weight(const std::string& syllable) const {
  const auto it = weights_.find(syllable);
  return it == weights_.end() ? 1.0 : it->second;
}

}  // namespace ninegrid
