#include <iostream>
#include <set>
#include <string>
#include <cassert>
#include <cstring> // 注意memset是cstring里的
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <exception>
#include <time.h>
#include "jsoncpp/json.h" // 在平台上，C++编译时默认包含此库
using std::cout;
using std::endl;
using std::exception;
using std::rand;
using std::set;
using std::sort;
using std::srand;
using std::string;
using std::time;
using std::unique;
using std::vector;
constexpr int PLAYER_COUNT = 3;
enum class Stage
{
    BIDDING, // 叫分阶段
    PLAYING  // 打牌阶段
};
enum class CardComboType
{
    PASS,       // 过
    SINGLE,     // 单张
    PAIR,       // 对子
    STRAIGHT,   // 顺子
    STRAIGHT2,  // 双顺
    TRIPLET,    // 三条
    TRIPLET1,   // 三带一
    TRIPLET2,   // 三带二
    BOMB,       // 炸弹
    QUADRUPLE2, // 四带二（只）
    QUADRUPLE4, // 四带二（对）
    PLANE,      // 飞机
    PLANE1,     // 飞机带小翼
    PLANE2,     // 飞机带大翼
    SSHUTTLE,   // 航天飞机
    SSHUTTLE2,  // 航天飞机带小翼
    SSHUTTLE4,  // 航天飞机带大翼
    ROCKET,     // 火箭
    INVALID     // 非法牌型
};
int cardComboScores[] = {
    0,  // 过
    1,  // 单张
    2,  // 对子
    6,  // 顺子
    6,  // 双顺
    4,  // 三条
    4,  // 三带一
    4,  // 三带二
    10, // 炸弹
    8,  // 四带二（只）
    8,  // 四带二（对）
    8,  // 飞机
    8,  // 飞机带小翼
    8,  // 飞机带大翼
    10, // 航天飞机（需要特判：二连为10分，多连为20分）
    10, // 航天飞机带小翼
    10, // 航天飞机带大翼
    16, // 火箭
    0   // 非法牌型
};
#ifndef _BOTZONE_ONLINE
string cardComboStrings[] = {
    "PASS",
    "SINGLE",
    "PAIR",
    "STRAIGHT",
    "STRAIGHT2",
    "TRIPLET",
    "TRIPLET1",
    "TRIPLET2",
    "BOMB",
    "QUADRUPLE2",
    "QUADRUPLE4",
    "PLANE",
    "PLANE1",
    "PLANE2",
    "SSHUTTLE",
    "SSHUTTLE2",
    "SSHUTTLE4",
    "ROCKET",
    "INVALID"};
#endif

// 用0~53这54个整数表示唯一的一张牌
using Card = short;
constexpr Card card_joker = 52;
constexpr Card card_JOKER = 53;

// 除了用0~53这54个整数表示唯一的牌，
// 这里还用另一种序号表示牌的大小（不管花色），以便比较，称作等级（Level）
// 对应关系如下：
// 3 4 5 6 7 8 9 10	J Q K	A	2	小王	大王
// 0 1 2 3 4 5 6 7	8 9 10	11	12	13	14
using Level = short;
constexpr Level MAX_LEVEL = 15;
constexpr Level MAX_STRAIGHT_LEVEL = 11;
constexpr Level level_joker = 13;
constexpr Level level_JOKER = 14;

/**
* 将Card变成Level
*/
constexpr Level card2level(Card card)
{
    return card / 4 + card / 53;
}

// 牌的组合，用于计算牌型
struct CardCombo
{
    // 表示同等级的牌有多少张
    // 会按个数从大到小、等级从大到小排序
    struct CardPack
    {
        Level level;
        short count;

        bool operator<(const CardPack &b) const
        {
            if (count == b.count)
                return level > b.level;
            return count > b.count;
        }
    };
    vector<Card> cards;      // 原始的牌，未排序
    vector<CardPack> packs;  // 按数目和大小排序的牌种
    CardComboType comboType; // 算出的牌型
    Level comboLevel = 0;    // 算出的大小序

    /**
						  * 检查个数最多的CardPack递减了几个
						  */
    int findMaxSeq() const
    {
        for (unsigned c = 1; c < packs.size(); c++)
            if (packs[c].count != packs[0].count ||
                packs[c].level != packs[c - 1].level - 1)
                return c;
        return packs.size();
    }

    /**
	* 这个牌型最后算总分的时候的权重
	*/
    int getWeight() const
    {
        if (comboType == CardComboType::SSHUTTLE ||
            comboType == CardComboType::SSHUTTLE2 ||
            comboType == CardComboType::SSHUTTLE4)
            return cardComboScores[(int)comboType] + (findMaxSeq() > 2) * 10;
        return cardComboScores[(int)comboType];
    }

    // 创建一个空牌组
    CardCombo() : comboType(CardComboType::PASS) {}

    /**
	* 通过Card（即short）类型的迭代器创建一个牌型
	* 并计算出牌型和大小序等
	* 假设输入没有重复数字（即重复的Card）
	*/
    template <typename CARD_ITERATOR>
    CardCombo(CARD_ITERATOR begin, CARD_ITERATOR end)
    {
        // 特判：空
        if (begin == end)
        {
            comboType = CardComboType::PASS;
            return;
        }

        // 每种牌有多少个
        short counts[MAX_LEVEL + 1] = {};

        // 同种牌的张数（有多少个单张、对子、三条、四条）
        short countOfCount[5] = {};
        cards = vector<Card>(begin, end);
        for (Card c : cards)
            counts[card2level(c)]++;
        for (Level l = 0; l <= MAX_LEVEL; l++)
            if (counts[l])
            {
                packs.push_back(CardPack{l, counts[l]});
                countOfCount[counts[l]]++;
            }
        sort(packs.begin(), packs.end());

        // 用最多的那种牌总是可以比较大小的
        comboLevel = packs[0].level;

        // 计算牌型
        // 按照 同种牌的张数 有几种 进行分类
        vector<int> kindOfCountOfCount;
        for (int i = 0; i <= 4; i++)
            if (countOfCount[i])
                kindOfCountOfCount.push_back(i);
        sort(kindOfCountOfCount.begin(), kindOfCountOfCount.end());

        int curr, lesser;

        switch (kindOfCountOfCount.size())
        {
        case 1: // 只有一类牌
            curr = countOfCount[kindOfCountOfCount[0]];
            switch (kindOfCountOfCount[0])
            {
            case 1:
                // 只有若干单张
                if (curr == 1)
                {
                    comboType = CardComboType::SINGLE;
                    return;
                }
                if (curr == 2 && packs[1].level == level_joker)
                {
                    comboType = CardComboType::ROCKET;
                    return;
                }
                if (curr >= 5 && findMaxSeq() == curr &&
                    packs.begin()->level <= MAX_STRAIGHT_LEVEL)
                {
                    comboType = CardComboType::STRAIGHT;
                    return;
                }
                break;
            case 2:
                // 只有若干对子
                if (curr == 1)
                {
                    comboType = CardComboType::PAIR;
                    return;
                }
                if (curr >= 3 && findMaxSeq() == curr &&
                    packs.begin()->level <= MAX_STRAIGHT_LEVEL)
                {
                    comboType = CardComboType::STRAIGHT2;
                    return;
                }
                break;
            case 3:
                // 只有若干三条
                if (curr == 1)
                {
                    comboType = CardComboType::TRIPLET;
                    return;
                }
                if (findMaxSeq() == curr &&
                    packs.begin()->level <= MAX_STRAIGHT_LEVEL)
                {
                    comboType = CardComboType::PLANE;
                    return;
                }
                break;
            case 4:
                // 只有若干四条
                if (curr == 1)
                {
                    comboType = CardComboType::BOMB;
                    return;
                }
                if (findMaxSeq() == curr &&
                    packs.begin()->level <= MAX_STRAIGHT_LEVEL)
                {
                    comboType = CardComboType::SSHUTTLE;
                    return;
                }
            }
            break;
        case 2: // 有两类牌
            curr = countOfCount[kindOfCountOfCount[1]];
            lesser = countOfCount[kindOfCountOfCount[0]];
            if (kindOfCountOfCount[1] == 3)
            {
                // 三条带？
                if (kindOfCountOfCount[0] == 1)
                {
                    // 三带一
                    if (curr == 1 && lesser == 1)
                    {
                        comboType = CardComboType::TRIPLET1;
                        return;
                    }
                    if (findMaxSeq() == curr && lesser == curr &&
                        packs.begin()->level <= MAX_STRAIGHT_LEVEL)
                    {
                        comboType = CardComboType::PLANE1;
                        return;
                    }
                }
                if (kindOfCountOfCount[0] == 2)
                {
                    // 三带二
                    if (curr == 1 && lesser == 1)
                    {
                        comboType = CardComboType::TRIPLET2;
                        return;
                    }
                    if (findMaxSeq() == curr && lesser == curr &&
                        packs.begin()->level <= MAX_STRAIGHT_LEVEL)
                    {
                        comboType = CardComboType::PLANE2;
                        return;
                    }
                }
            }
            if (kindOfCountOfCount[1] == 4)
            {
                // 四条带？
                if (kindOfCountOfCount[0] == 1)
                {
                    // 四条带两只 * n
                    if (curr == 1 && lesser == 2)
                    {
                        comboType = CardComboType::QUADRUPLE2;
                        return;
                    }
                    if (findMaxSeq() == curr && lesser == curr * 2 &&
                        packs.begin()->level <= MAX_STRAIGHT_LEVEL)
                    {
                        comboType = CardComboType::SSHUTTLE2;
                        return;
                    }
                }
                if (kindOfCountOfCount[0] == 2)
                {
                    // 四条带两对 * n
                    if (curr == 1 && lesser == 2)
                    {
                        comboType = CardComboType::QUADRUPLE4;
                        return;
                    }
                    if (findMaxSeq() == curr && lesser == curr * 2 &&
                        packs.begin()->level <= MAX_STRAIGHT_LEVEL)
                    {
                        comboType = CardComboType::SSHUTTLE4;
                        return;
                    }
                }
            }
        }

        comboType = CardComboType::INVALID;
    }

    /**
	* 判断指定牌组能否大过当前牌组（这个函数不考虑过牌的情况！）
	*/
    bool canBeBeatenBy(const CardCombo &b) const
    {
        if (comboType == CardComboType::INVALID || b.comboType == CardComboType::INVALID)
            return false;
        if (b.comboType == CardComboType::ROCKET)
            return true;
        if (b.comboType == CardComboType::BOMB)
            switch (comboType)
            {
            case CardComboType::ROCKET:
                return false;
            case CardComboType::BOMB:
                return b.comboLevel > comboLevel;
            default:
                return true;
            }
        return b.comboType == comboType && b.cards.size() == cards.size() && b.comboLevel > comboLevel;
    }

    /**
	* 从指定手牌中寻找第一个能大过当前牌组的牌组
	* 如果随便出的话只出第一张
	* 如果不存在则返回一个PASS的牌组
	*/
    template <typename CARD_ITERATOR>
    CardCombo findFirstValid(CARD_ITERATOR begin, CARD_ITERATOR end) const
    {
        if (comboType == CardComboType::PASS) // 如果不需要大过谁，只需要随便出
        {
            CARD_ITERATOR second = begin;
            second++;
            return CardCombo(begin, second); // 那么就出第一张牌……
        }

        // 然后先看一下是不是火箭，是的话就过
        if (comboType == CardComboType::ROCKET)
            return CardCombo();

        // 现在打算从手牌中凑出同牌型的牌
        auto deck = vector<Card>(begin, end); // 手牌
        short counts[MAX_LEVEL + 1] = {};

        unsigned short kindCount = 0;

        // 先数一下手牌里每种牌有多少个
        for (Card c : deck)
            counts[card2level(c)]++;

        // 手牌如果不够用，直接不用凑了，看看能不能炸吧
        if (deck.size() < cards.size())
            goto failure;

        // 再数一下手牌里有多少种牌
        for (short c : counts)
            if (c)
                kindCount++;

        // 否则不断增大当前牌组的主牌，看看能不能找到匹配的牌组
        {
            // 开始增大主牌
            int mainPackCount = findMaxSeq();
            bool isSequential =
                comboType == CardComboType::STRAIGHT ||
                comboType == CardComboType::STRAIGHT2 ||
                comboType == CardComboType::PLANE ||
                comboType == CardComboType::PLANE1 ||
                comboType == CardComboType::PLANE2 ||
                comboType == CardComboType::SSHUTTLE ||
                comboType == CardComboType::SSHUTTLE2 ||
                comboType == CardComboType::SSHUTTLE4;
            for (Level i = 1;; i++) // 增大多少
            {
                for (int j = 0; j < mainPackCount; j++)
                {
                    int level = packs[j].level + i;

                    // 各种连续牌型的主牌不能到2，非连续牌型的主牌不能到小王，单张的主牌不能超过大王
                    if ((comboType == CardComboType::SINGLE && level > MAX_LEVEL) ||
                        (isSequential && level > MAX_STRAIGHT_LEVEL) ||
                        (comboType != CardComboType::SINGLE && !isSequential && level >= level_joker))
                        goto failure;

                    // 如果手牌中这种牌不够，就不用继续增了
                    if (counts[level] < packs[j].count)
                        goto next;
                }

                {
                    // 找到了合适的主牌，那么从牌呢？
                    // 如果手牌的种类数不够，那从牌的种类数就不够，也不行
                    if (kindCount < packs.size())
                        continue;

                    // 好终于可以了
                    // 计算每种牌的要求数目吧
                    short requiredCounts[MAX_LEVEL + 1] = {};
                    for (int j = 0; j < mainPackCount; j++)
                        requiredCounts[packs[j].level + i] = packs[j].count;
                    for (unsigned j = mainPackCount; j < packs.size(); j++)
                    {
                        Level k;
                        for (k = 0; k <= MAX_LEVEL; k++)
                        {
                            if (requiredCounts[k] || counts[k] < packs[j].count)
                                continue;
                            requiredCounts[k] = packs[j].count;
                            break;
                        }
                        if (k == MAX_LEVEL + 1) // 如果是都不符合要求……就不行了
                            goto next;
                    }

                    // 开始产生解
                    vector<Card> solve;
                    for (Card c : deck)
                    {
                        Level level = card2level(c);
                        if (requiredCounts[level])
                        {
                            solve.push_back(c);
                            requiredCounts[level]--;
                        }
                    }
                    return CardCombo(solve.begin(), solve.end());
                }

            next:; // 再增大
            }
        }

    failure:
        // 实在找不到啊
        // 最后看一下能不能炸吧

        for (Level i = 0; i < level_joker; i++)
            if (counts[i] == 4 && (comboType != CardComboType::BOMB || i > packs[0].level)) // 如果对方是炸弹，能炸的过才行
            {
                // 还真可以啊……
                Card bomb[] = {Card(i * 4), Card(i * 4 + 1), Card(i * 4 + 2), Card(i * 4 + 3)};
                return CardCombo(bomb, bomb + 4);
            }

        // 有没有火箭？
        if (counts[level_joker] + counts[level_JOKER] == 2)
        {
            Card rocket[] = {card_joker, card_JOKER};
            return CardCombo(rocket, rocket + 2);
        }

        // ……
        return CardCombo();
    }

    void debugPrint()
    {
#ifndef _BOTZONE_ONLINE
        std::cout << "【" << cardComboStrings[(int)comboType] << "共" << cards.size() << "张，大小序" << comboLevel << "】";
#endif
    }
};

// 我的牌有哪些
set<Card> myCards;

// 地主被明示的牌有哪些
set<Card> landlordPublicCards;

// 大家从最开始到现在都出过什么
vector<vector<Card>> whatTheyPlayed[PLAYER_COUNT];

// 当前要出的牌需要大过谁
CardCombo lastValidCombo;

// 大家还剩多少牌
short cardRemaining[PLAYER_COUNT] = {20, 17, 17};

// 我是几号玩家（0-地主，1-农民甲，2-农民乙）
int myPosition;

// 地主位置
int landlordPosition = -1;

// 地主叫分
int landlordBid = -1;

// 阶段
Stage stage = Stage::BIDDING;
//输出叫分
vector<int> bidInput;

int last_player = 0, who_last_player = 0; //1队友（农民才有），2对手（地主），0自己
short Curmycard[MAX_LEVEL];
int Cardnum;
double maxscore = 1e10;
int leasttimes = 1e9, max_times = 1e9;
int HowManyTimes = 0;
vector<Card> bestcard;
void evaluate_prepare()
{
    memset(Curmycard, 0, sizeof(Curmycard));
    for (auto c : myCards)
    {
        Curmycard[card2level(c)]++;
    }
    Cardnum = myCards.size();
    max_times = leasttimes = 1e9;
    maxscore = 1e10;
    HowManyTimes = 0;
    return;
}
set<Card> remain_card; //全场还没出的牌，以此为依据判断对手的牌型
bool judge_last_two()
{

    int Curmycard_copy[15];
    int cardnum_copy;
    for (int i = 0; i <= 14; i++)
        Curmycard_copy[i] = Curmycard[i];
    cardnum_copy = Cardnum;

    int count = 0;
    //先王炸
    if (Curmycard_copy[13] == 1 && Curmycard_copy[14] == 1)
    {

        Curmycard_copy[13] = 0;
        Curmycard_copy[14] = 0;
        cardnum_copy -= 2;
        count++;
    }
    //炸弹
    for (int i = 0; i <= 12; i++)
    {
        if (Curmycard_copy[i] == 4)
        {
            Curmycard_copy[i] -= 4;
            count++;
            cardnum_copy -= 4;

            break;
        }
    }
    //双顺
    int chain = 0;
    for (int i = 0; i <= 12; i++)
    {

        if (Curmycard_copy[i] == 2)
        {
            chain++;
        }
        if (Curmycard_copy[i] < 2)
        {
            if (chain >= 3)
            {

                for (int j = i - chain; j < i; j++)
                {
                    Curmycard_copy[j] = 0;
                }

                cardnum_copy -= chain;
                count++;

                break;
            }
            chain = 0;
        }
    }
    if (count <= 2 && cardnum_copy == 0)
    {

        return true;
    }
    if (count > 2)
        return false;
    //单顺
    chain = 0;
    for (int i = 0; i <= 12; i++)
    {

        if (Curmycard_copy[i] >= 1)
            chain++;
        else
        {
            if (chain >= 5)
            {
                for (int j = i - chain; j < i; j++)
                    Curmycard_copy[j]--;
                cardnum_copy -= chain;
                count++;

                chain = 0;
            }
        }
    }
    if (count <= 2 && cardnum_copy == 0)
    {

        return true;
    }
    if (count > 2)
        return false;
    //三带(飞机暂时不考虑)
    int three = -1;
    for (int i = 0; i <= 12; i++)
    {

        if (Curmycard_copy[i] == 3)
        {
            three = i;
            break;
        }
    }
    if (three != -1) //优先三带二
    {
        for (int i = 0; i <= 14; i++)
        {
            if (Curmycard_copy[i] == 1 || Curmycard_copy[i] == 2)
            {
                int tmp = Curmycard_copy[i];
                cardnum_copy -= Curmycard_copy[i];
                Curmycard_copy[i] = 0;
                Curmycard_copy[three] = 0;
                cardnum_copy -= 3;
                count++;

                break;
            }
        }
    }
    if (count <= 2 && cardnum_copy == 0)
    {

        return true;
    }
    if (count > 2)
        return false;
    //找对子
    for (int i = 0; i <= 12; i++)
    {
        if (Curmycard_copy[i] == 2)
        {
            Curmycard_copy[i] = 0;
            cardnum_copy -= 2;
            count++;

            if (count > 2)
                return false;
        }
    }
    if (count <= 2 && cardnum_copy == 0)
    {

        return true;
    }
    if (count > 2)
        return false;
    //最后，单根
    for (int i = 0; i <= 14; i++)
    {
        if (Curmycard_copy[i] == 1)
        {
            Curmycard_copy[i] = 0;
            cardnum_copy--;
            count++;

            if (count > 2)
                return false;
        }
    }
    if (count <= 2 && cardnum_copy == 0)
    {

        return true;
    }
    else
        return false;
}
//寻找顺子，分别取最长的k顺子
int straight_k[4] = {0, 5, 3, 1};
int find_straight(int &s, int k, int begin = 0)
{
    int j, i;
    for (i = begin; i <= 10;)
    {
        while (i <= 10 && Curmycard[i] < k)
            ++i;
        j = 1;
        while (i + j <= 11 && Curmycard[i + j] >= k)
            ++j;
        if (j >= straight_k[k] && i <= 10)
        {
            s = j;
            return i;
        }
        i = i + j;
    }
    s = 0;
    return 0;
}

//判断是否绝杀
bool judge_best()
{
    if (CardCombo(bestcard.begin(), bestcard.end()).findFirstValid(remain_card.begin(), remain_card.end()).comboType == CardComboType::PASS)
        return true;
    else
        return false;
}

//附加分，主要看场上的客观条件和上下家关系的因素
double extra_point(const int num, const int i)
{
    if (judge_best())
    {
        int n = 0;
        for (int i = 0; i < MAX_LEVEL; ++i)
            if (Curmycard[i] > 0)
                ++n;
        if (n <= 1)
            return -1e9;
    }
    double extra = 0;
    if (myPosition == 0)
    {
        if (cardRemaining[1] < 3 && num == cardRemaining[1])
            extra += (16 - i) / 10.0;
        if (cardRemaining[2] < 3 && num == cardRemaining[2])
            extra += (16 - i) / 10.0;
    }
    else
    {
        if (cardRemaining[0] < 3 && num == cardRemaining[0] && last_player == 2)
            extra += (16 - i) / 10.0;
    }
    return extra;
}

//手牌全体评估函数
double evaluate(int depth, int kth, double &score)
{

    if (Cardnum <= 0 || kth == 6)
    {
        score += 0.1 * HowManyTimes;
        if (maxscore > score)
        {
            maxscore = score;
            max_times = HowManyTimes;
        }
        if (HowManyTimes < leasttimes)
            leasttimes = HowManyTimes;
        score -= 0.1 * HowManyTimes;
        return 0;
    }
    switch (kth)
    {
    case 0:
        //找炸弹
        for (int i = 0; i < MAX_LEVEL - 2; ++i)
        {
            if (Curmycard[i] == 4)
            {
                score--;
                //炸弹绝杀，分数低
                Curmycard[i] = 0;
                Cardnum -= 4;
                HowManyTimes++; //一手牌
                evaluate(depth + 1, 0, score);
                HowManyTimes--; //一手牌
                Curmycard[i] = 4;
                Cardnum += 4;
                score++;
            }
        }
        if (Curmycard[MAX_LEVEL - 2] == 1 && Curmycard[MAX_LEVEL - 1] == 1) //王炸
        {
            score -= 1.2;
            Curmycard[MAX_LEVEL - 2] = 0;
            Curmycard[MAX_LEVEL - 1] = 0;
            Cardnum -= 2;
            HowManyTimes++; //一手牌
            evaluate(depth + 1, 1, score);
            HowManyTimes--; //一手牌
            Curmycard[MAX_LEVEL - 2] = 1;
            Curmycard[MAX_LEVEL - 1] = 1;
            Cardnum += 2;
        }

    case 1:
        //顺子
        {
            int s = 0;
            int i = find_straight(s, 1);
            if (s >= 5)
            {
                for (int b = i; b <= i + s - 5; ++b)
                {
                    for (int k = 5; k <= s && b + k <= i + s; ++k)
                    {
                        for (int j = 0; j < k; ++j)
                        {
                            Curmycard[b + j]--;
                        }
                        Cardnum -= k;
                        score += 0.5;
                        HowManyTimes++; //一手牌
                        evaluate(depth + 1, 1, score);
                        HowManyTimes--; //一手牌
                        for (int j = 0; j < k; ++j)
                        {
                            Curmycard[b + j]++;
                        }
                        Cardnum += k;
                        score -= 0.5;
                    }
                }
            }
        }

    case 2:
        //找对顺
        {
            int s = 0;
            int i = find_straight(s, 2);
            if (s != 0)
            {
                for (int b = i; b <= i + s - 3; ++b)
                {
                    for (int k = 3; k <= s && b + k <= i + s; ++k)
                    {
                        for (int j = 0; j < k; ++j)
                        {
                            Curmycard[b + j] -= 2;
                        }
                        Cardnum -= k * 2;
                        score += 0.5;
                        HowManyTimes++; //一手牌
                        evaluate(depth + 1, 2, score);
                        HowManyTimes--; //一手牌
                        for (int j = 0; j < k; ++j)
                        {
                            Curmycard[b + j] += 2;
                        }
                        Cardnum += k * 2;
                        score -= 0.5;
                    }
                }
            }
        }

    case 3:
        //飞机或三条
        {
            int s = 0;
            int i = find_straight(s, 3);

            int wings[6], pair = 0;
            int wing[6], single = 0;
            //有三条
            if (s != 0)
            {
                for (int j = 0; j < s; ++j)
                {
                    Curmycard[i + j] -= 3;
                }
                Cardnum -= 3 * s;
                for (int t = 0; t < 12 && (t < i || t >= i + s); ++t)
                {
                    if (Curmycard[t] == 1 && single < s)
                    {
                        wing[single] = t;
                        single++;
                    }
                    else if (Curmycard[t] == 2 && pair < s)
                    {
                        wings[pair] = t;
                        pair++;
                    }
                }
                //飞机带小翼
                if (single == s)
                {
                    for (int o = 0; o < s; ++o)
                    {
                        Curmycard[wing[o]] = 0;
                    }
                    Cardnum -= s;
                    if (i <= 7)
                    {
                        score += 1;
                    }
                    else if (s == 1)
                    {
                        score += 0.5;
                    }
                    else
                    {
                        score += 0.2;
                    }
                    HowManyTimes++; //一手牌
                    evaluate(depth + 1, 3, score);
                    HowManyTimes--; //一手牌
                    for (int o = 0; o < s; ++o)
                    {
                        Curmycard[wing[o]] = 1;
                    }
                    Cardnum += s;
                    if (i <= 7)
                    {
                        score -= 1;
                    }
                    else if (s == 1)
                    {
                        score -= 0.5;
                    }
                    else
                    {
                        score -= 0.2;
                    }
                }

                //飞机带大翼
                if (pair == s)
                {
                    for (int o = 0; o < s; ++o)
                    {
                        Curmycard[wings[o]] = 0;
                    }
                    Cardnum -= s * 2;
                    if (i <= 7)
                    {
                        score += 1;
                    }
                    else if (s == 1)
                    {
                        score += 0.5;
                    }
                    else
                    {
                        score += 0.2;
                    }
                    HowManyTimes++; //一手牌
                    evaluate(depth + 1, 3, score);
                    HowManyTimes--; //一手牌
                    for (int o = 0; o < s; ++o)
                    {
                        Curmycard[wings[o]] = 2;
                    }
                    Cardnum += s * 2;
                    if (i <= 7)
                    {
                        score -= 1;
                    }
                    else if (s == 1)
                    {
                        score -= 0.5;
                    }
                    else
                    {
                        score -= 0.2;
                    }
                }

                //飞机不带翼
                if (i <= 7)
                {
                    score += 1;
                }
                else if (s == 1)
                {
                    score += 0.5;
                }
                else
                {
                    score += 0.2;
                }
                HowManyTimes++; //一手牌
                evaluate(depth + 1, 3, score);
                HowManyTimes--; //一手牌
                if (i <= 7)
                {
                    score -= 1;
                }
                else if (s == 1)
                {
                    score -= 0.5;
                }
                else
                {
                    score -= 0.2;
                }
                for (int j = 0; j < s; ++j)
                {
                    Curmycard[i + j] += 3;
                }
                Cardnum += 3 * s;
            }
        }

    case 4:
        //四带二
        {
            for (int i = 0; i <= 12; ++i)
            {
                if (Curmycard[i] == 4)
                {
                    Curmycard[i] = 0;
                    Cardnum -= 4;
                    int _single[2], single = 0;
                    int _pair[2], pair = 0;
                    for (int t = 0; t < 12; ++t)
                    {
                        if (Curmycard[t] == 1 && single < 2)
                        {
                            _single[single] = t;
                            single++;
                        }
                        else if (Curmycard[t] == 2 && pair < 2)
                        {
                            _pair[pair] = t;
                            pair++;
                        }
                    }
                    //四带二单
                    if (single == 2)
                    {
                        Curmycard[_single[0]] = Curmycard[_single[1]] = 0;
                        Cardnum -= 2;
                        score += 0.1;
                        HowManyTimes++; //最后一手牌
                        evaluate(depth + 1, 4, score);
                        HowManyTimes--; //最后一手牌
                        Curmycard[_single[0]] = Curmycard[_single[1]] = 1;
                        Cardnum += 2;
                        score -= 0.1;
                    }
                    //四带两对
                    if (pair == 2)
                    {
                        Curmycard[_pair[0]] = Curmycard[_pair[1]] = 0;
                        Cardnum -= 4;
                        score += 0;
                        HowManyTimes++; //打出一手牌
                        evaluate(depth + 1, 4, score);
                        HowManyTimes--; //打出一手牌
                        Curmycard[_pair[0]] = Curmycard[_pair[1]] = 2;
                        Cardnum += 4;
                        score -= 0;
                    }
                }
            }
        }

    case 5:
        //对子 or 单牌
        {
            double t = score;
            int n = 0;
            for (int i = 0; i < MAX_LEVEL - 5; ++i)
            {
                if (Curmycard[i] != 0)
                {
                    score += 1;
                    ++n;
                }
            }

            for (int i = MAX_LEVEL - 5; i < MAX_LEVEL - 3; ++i)
            {
                if (Curmycard[i] != 0)
                {
                    score = score;
                    ++n;
                }
            }
            for (int i = MAX_LEVEL - 3; i < MAX_LEVEL - 1; ++i) //2或者王
            {
                if (Curmycard[i] != 0)
                {
                    score = score - 0.5;
                    ++n;
                }
            }
            if (Curmycard[MAX_LEVEL - 1] != 0)
            {
                score -= 1;
                ++n;
            }
            HowManyTimes += n;
            evaluate(depth, 6, score);
            HowManyTimes -= n;
            score = t;
        }
    default:
        break;
    }
    return maxscore;
}

void pushcard(Level l, short c)
{
    if (l == MAX_LEVEL - 2 || l == MAX_LEVEL - 1)
    {
        bestcard.push_back(54 - (MAX_LEVEL - l));
        return;
    }
    for (short i = l * 4; i < l * 4 + 4; ++i)
    {
        if (myCards.find(i) != myCards.end() && c > 0)
        {
            bestcard.push_back(Card(i));
            c--;
        }
    }
    return;
}

CardCombo findBestValid()
{
    if (lastValidCombo.comboType == CardComboType::ROCKET) //别人如果王炸就过掉
        return CardCombo();

    if (lastValidCombo.comboType == CardComboType::PASS)
        last_player = 2;
    else if (myPosition == 0)
    {
        last_player = 0;
        if (whatTheyPlayed[(myPosition + 2) % 3].back().size() != 0)
            who_last_player = (myPosition + 2) % 3;
        else
            who_last_player = (myPosition + 1) % 3;
    }
    else if (whatTheyPlayed[(myPosition + 2) % 3].back().size() != 0)
    {
        who_last_player = (myPosition + 2) % 3;
        if (myPosition + who_last_player == 3)
            last_player = 1;
        else
            last_player = 0;
    }
    else
    {
        who_last_player = (myPosition + 1) % 3;
        if (myPosition + who_last_player == 3)
            last_player = 1;
        else
            last_player = 0;
    }

    bestcard.clear();
    evaluate_prepare();
    double value = 1e9;
    double score = 0;
    double cur_score = evaluate(0, 0, score);

    if (lastValidCombo.comboType != CardComboType::PASS)
    {
        //被动出牌
        int i = 0;
        bestcard.clear();
        //炸弹，看是否比对面更大
        if (lastValidCombo.comboType == CardComboType::BOMB)
        {
            i = lastValidCombo.comboLevel + 1;
        }

        for (; i < MAX_LEVEL - 2; ++i)
        {
            evaluate_prepare();
            if (Curmycard[i] == 4)
            {
                Curmycard[i] = 0;
                Cardnum -= 4;
                double score = 0;
                double temp = evaluate(0, 0, score);
                Curmycard[i] = 4;
                Cardnum += 4;
                if (value > temp)
                {
                    bestcard.clear();
                    value = temp;
                    for (int j = 0; j < 4; ++j)
                        bestcard.push_back(Card(4 * i + j));
                }
            }
        }
        if (Curmycard[MAX_LEVEL - 2] == 1 && Curmycard[MAX_LEVEL - 1] == 1) //出王炸你怕不怕
        {
            evaluate_prepare();
            Curmycard[MAX_LEVEL - 2] = Curmycard[MAX_LEVEL - 1] = 0;
            Cardnum -= 2;
            double score = 0;
            double temp = evaluate(0, 0, score);
            Curmycard[MAX_LEVEL - 2] = Curmycard[MAX_LEVEL - 1] = 1;
            Cardnum += 2;
            if (value > temp)
            {
                bestcard.clear();
                value = temp;
                for (int j = 0; j < 2; ++j)
                    bestcard.push_back(Card(52 + j));
            }
        }

        //必须出单张
        if (lastValidCombo.comboType == CardComboType::SINGLE)
        {
            evaluate_prepare();
            for (int i = lastValidCombo.comboLevel + 1; i < MAX_LEVEL; ++i)
            {
                if (Curmycard[i] != 0)
                {
                    evaluate_prepare();
                    Curmycard[i]--;
                    Cardnum--;
                    double score = 0;
                    double temp = evaluate(0, 0, score);
                    temp += extra_point(1, i);
                    if (Cardnum == 0) //能一下出完，太棒了！
                        temp = -1e9;
                    Curmycard[i]++;
                    Cardnum++;
                    if (value > temp)
                    {
                        value = temp;
                        bestcard.clear();
                        pushcard(i, 1);
                    }
                }
            }
        }
        //必须出对子
        else if (lastValidCombo.comboType == CardComboType::PAIR)
        {
            evaluate_prepare();
            int pair = 0;
            for (int i = lastValidCombo.comboLevel + 1; i < MAX_LEVEL - 2; ++i)
            {
                if (Curmycard[i] >= 2)
                {
                    evaluate_prepare();
                    Curmycard[i] -= 2;
                    Cardnum -= 2;
                    double score = 0;
                    double temp = evaluate(0, 0, score);
                    temp += extra_point(2, i);
                    if (Cardnum == 0) //能一下出完，太棒了！
                        temp = -1e9;
                    Curmycard[i] += 2;
                    Cardnum += 2;
                    if (value > temp)
                    {
                        value = temp;
                        bestcard.clear();
                        pushcard(i, 2);
                    }
                }
            }
        }

        //是单顺或双顺
        else if (lastValidCombo.comboType == CardComboType::STRAIGHT || lastValidCombo.comboType == CardComboType::STRAIGHT2)
        {
            evaluate_prepare();
            int c;
            if (lastValidCombo.comboType == CardComboType::STRAIGHT) //单顺
                c = 1;
            else //双顺
                c = 2;
            int s = lastValidCombo.cards.size() / c;
            for (int i = lastValidCombo.comboLevel + 1; i <= MAX_LEVEL - 3 - s; ++i)
            {
                int j = 0;
                while (i + j <= MAX_LEVEL - 3 && Curmycard[i + j] >= c)
                {
                    j++;
                }
                if (j >= s)
                {
                    evaluate_prepare();
                    for (int k = 0; k <= j - s; ++k)
                    {
                        for (int t = k; t < s + k; ++t)
                        {
                            Curmycard[i + t] -= c;
                        }
                        Cardnum -= s * c;
                        double score = 0;
                        double temp = evaluate(0, 0, score);
                        temp += extra_point(5, i);
                        if (Cardnum == 0)
                            temp = -1e9;
                        for (int t = k; t < s + k; ++t)
                        {
                            Curmycard[i + t] += c;
                        }
                        Cardnum += s * c;
                        if (value > temp)
                        {
                            value = temp;
                            bestcard.clear();
                            for (int t = k; t < s + k; ++t)
                            {
                                pushcard(i + t, c);
                            }
                        }
                    }
                }
                i = i + j + 1;
            }
        }

        //三条
        else if (lastValidCombo.comboType == CardComboType::TRIPLET || lastValidCombo.comboType == CardComboType::TRIPLET2 || lastValidCombo.comboType == CardComboType::TRIPLET1 ||
                 lastValidCombo.comboType == CardComboType::PLANE || lastValidCombo.comboType == CardComboType::PLANE2 || lastValidCombo.comboType == CardComboType::PLANE1)
        {
            evaluate_prepare();
            int c, s;                                                                                                                                                             //分别是coumputer和science[/huaji]
            if (lastValidCombo.comboType == CardComboType::TRIPLET || lastValidCombo.comboType == CardComboType::TRIPLET2 || lastValidCombo.comboType == CardComboType::TRIPLET1) //三个头
            {
                c = lastValidCombo.cards.size() - 3;
                s = 1;
            }
            else //飞机
            {
                if (lastValidCombo.packs.back().count != 3)
                    s = lastValidCombo.packs.size() / 2;
                else
                    s = lastValidCombo.packs.size();
                c = (lastValidCombo.cards.size() - s * 3) / s;
            }

            for (int i = lastValidCombo.comboLevel + 2 - s; i <= MAX_LEVEL - 2 - s;)
            {
                int j = 0;
                while (i + j < MAX_LEVEL - 2 && Curmycard[i + j] == 3)
                {
                    j++;
                }
                if (j >= s) //找到了三条
                {
                    for (int t = 0; t <= j - s; ++t)
                    {
                        evaluate_prepare();
                        int sc[4], single = 0;
                        for (int k = i + t; k < i + s + t; ++k)
                        {
                            Curmycard[k] -= 3;
                            Cardnum -= 3;
                        }
                        for (int k = 0; k < MAX_LEVEL; ++k)
                        {
                            if (c == 0 || single == s)
                                break;
                            if (Curmycard[k] == c && single < s && (k < i + t || k >= i + s + t))
                            {
                                sc[single] = k;
                                single++;
                                Curmycard[single] -= c;
                                Cardnum -= c;
                            }
                        }
                        if (c != 0 && single < s)
                        {
                            for (int k = 0; k < MAX_LEVEL; ++k)
                            {
                                if (c == 0)
                                    break;
                                if (Curmycard[k] >= c && single < s && (k < i + t || k >= i + s + t))
                                {
                                    sc[single] = k;
                                    single++;
                                    Curmycard[single] -= c;
                                    Cardnum -= c;
                                }
                            }
                        }
                        if (c != 0 && single < s)
                        {
                            break;
                        }

                        double score = 0;
                        double temp = evaluate(0, 0, score);
                        temp += extra_point(4, i);
                        if (Cardnum == 0)
                            temp = -1e9;
                        if (value > temp)
                        {
                            value = temp;
                            bestcard.clear();
                            for (int k = i + t; k < s + i + t; ++k)
                            {
                                pushcard(k, 3);
                            }
                            if (c != 0)
                                for (int t = 0; t < s; ++t)
                                {
                                    pushcard(sc[t], c);
                                }
                        }
                        for (int k = i + t; k < i + s + t; ++k)
                        {
                            Curmycard[k] += 3;
                            Cardnum += 3;
                        }
                    }
                }
                i = i + j + 1;
            }
        }
        //四带二
        else if (lastValidCombo.comboType == CardComboType::QUADRUPLE2 || lastValidCombo.comboType == CardComboType::QUADRUPLE4)
        {
            int c;
            evaluate_prepare();
            if (lastValidCombo.comboType == CardComboType::QUADRUPLE2)
            {
                c = 1;
            }
            else
                c = 2;
            for (int i = lastValidCombo.comboLevel + 1; i < MAX_LEVEL; ++i)
            {
                if (Curmycard[i] == 4)
                {
                    //如果可以单牌或对子全搜一遍就更好了
                    evaluate_prepare();
                    Curmycard[i] -= 4;
                    Cardnum -= 4;
                    int sc[2], single = 0;
                    for (int k = 0; k < MAX_LEVEL - 3; ++k)
                    {
                        if (Curmycard[k] == c && single < 2)
                        {
                            sc[single] = k;
                            single++;
                            Curmycard[k] -= c;
                            Cardnum -= c;
                        }
                    }
                    if (single < 2)
                    {
                        i = MAX_LEVEL;
                        break;
                    }

                    double score = 0;
                    double temp = evaluate(0, 0, score);
                    temp += extra_point(6, i);
                    if (Cardnum == 0)
                        temp = -1e9;
                    if (value > temp)
                    {
                        bestcard.clear();
                        value = temp;
                        for (int j = 0; j < 4; ++j)
                            bestcard.push_back(Card(4 * i + j));
                        for (int j = 0; j < 2; ++j)
                            pushcard(sc[j], c);
                        break;
                    }
                }
            }
        }
    }
    else
    {
        if (judge_last_two())
        {
            int Curmycard_copy[15];
            int cardnum_copy;
            for (int i = 0; i <= 14; i++)
                Curmycard_copy[i] = Curmycard[i];
            cardnum_copy = Cardnum;
            //火箭
            if (Curmycard_copy[13] == 1 && Curmycard_copy[14] == 1)
            {

                Curmycard_copy[13] = 0;
                Curmycard_copy[14] = 0;
                cardnum_copy -= 2;
                bestcard.clear(); //需要clear，因为push_back里面不会去清空
                pushcard(13, 1);
                pushcard(14, 1);
                value = -1e9;
                goto flag;
            }
            //炸弹
            for (int i = 0; i <= 12; i++)
            {
                if (Curmycard_copy[i] == 4)
                {
                    Curmycard_copy[i] -= 4;

                    cardnum_copy -= 4;

                    bestcard.clear();
                    pushcard(i, 4);
                    value = -1e9;
                    goto flag;
                }
            }
            //双顺
            int chain = 0;
            for (int i = 0; i <= 12; i++)
            {

                if (Curmycard_copy[i] == 2)
                {
                    chain++;
                }
                if (Curmycard_copy[i] < 2)
                {
                    if (chain >= 3)
                    {

                        for (int j = i - chain; j < i; j++)
                        {
                            Curmycard_copy[j] = 0;
                        }

                        bestcard.clear();
                        for (int j = i - chain; j < i; j++)
                        {
                            pushcard(j, 2);
                        }

                        cardnum_copy -= chain;
                        value = -1e9;
                        goto flag;
                    }
                    chain = 0;
                }
            }
            chain = 0;
            for (int i = 0; i <= 12; i++)
            {

                if (Curmycard_copy[i] >= 1)
                    chain++;
                else
                {
                    if (chain >= 5)
                    {
                        for (int j = i - chain; j < i; j++)
                            Curmycard_copy[j]--;
                        cardnum_copy -= chain;

                        bestcard.clear();
                        for (int j = i - chain; j < i; j++)
                        {
                            pushcard(j, 1);
                        }
                        value = -1e9;
                        goto flag;
                        chain = 0;
                    }
                    chain = 0;
                }
            }
            //三带(飞机暂时不考虑)
            int three = -1;
            for (int i = 12; i >= 0; i--)
            {

                if (Curmycard_copy[i] == 3)
                {
                    three = i;
                    break;
                }
            }

            if (three != -1) //优先三带二
            {
                for (int i = 0; i <= 14; i++)
                {
                    if (Curmycard_copy[i] == 1 || Curmycard_copy[i] == 2)
                    {
                        int tmp = Curmycard_copy[i];
                        cardnum_copy -= Curmycard_copy[i];
                        Curmycard_copy[i] = 0;
                        Curmycard_copy[three] = 0;
                        cardnum_copy -= 3;

                        bestcard.clear();
                        pushcard(i, tmp);
                        pushcard(three, 3);
                        value = -1e9;
                        goto flag;

                        break;
                    }
                }
            }
            //对子
            for (int i = 12; i >= 0; i--)
            {
                if (Curmycard_copy[i] == 2)
                {
                    Curmycard_copy[i] = 0;
                    cardnum_copy -= 2;

                    bestcard.clear();
                    pushcard(i, 2);
                    value = -1e9;
                    goto flag;
                    break;
                }
            }
            //最后，单根
            for (int i = 14; i >= 0; i--)
            {
                if (Curmycard_copy[i] == 1)
                {
                    Curmycard_copy[i] = 0;
                    cardnum_copy--;

                    pushcard(i, 1);
                    value = -1e9;
                    goto flag;
                }
            }
        }
        //主动出牌
        //出顺子吗
        else
        {
            {
                evaluate_prepare();
                int s = 0;
                int i = find_straight(s, 1);
                if (s >= 5)
                {
                    for (int b = i; b <= i + s - 5; ++b)
                    {
                        for (int k = 5; k <= s && b + k <= i + s; ++k)
                        {
                            for (int j = 0; j < k; ++j)
                            {
                                Curmycard[b + j]--;
                            }
                            Cardnum -= s;
                            double score = 0;
                            double temp = evaluate(0, 0, score);
                            temp += extra_point(5, b);
                            if (b + s < 11)
                                temp = temp - 0.8;
                            else
                                temp = temp - 0.5;
                            if (Cardnum == 0) //能一下出完，太棒了！
                                temp = -1e9;
                            //  if(last_one_hand(Curmycard))
                            //      temp=temp-1;
                            for (int j = 0; j < k; ++j)
                            {
                                Curmycard[b + j]++;
                            }
                            Cardnum += s;
                            if (value > temp)
                            {
                                value = temp;
                                bestcard.clear();
                                for (int t = 0; t < s; ++t)
                                {
                                    pushcard(i + t, 1);
                                }
                            }
                        }
                    }
                }
            }
            //有单张出单张
            {
                int single = 0;
                evaluate_prepare();
                for (int i = 0; i < MAX_LEVEL; ++i)
                {
                    if (Curmycard[i] != 0)
                    {
                        evaluate_prepare();
                        Curmycard[i]--;
                        Cardnum--;
                        double score = 0;
                        double temp = evaluate(0, 0, score);
                        temp += extra_point(1, i) - 0.05 + i / 200.0;
                        Curmycard[i]++;
                        Cardnum++;
                        if (value > temp)
                        {
                            value = temp;
                            bestcard.clear();
                            pushcard(i, 1);
                        }
                    }
                }
            }

            //一对
            {
                int pair = 0;
                evaluate_prepare();
                for (int i = 0; i < MAX_LEVEL; ++i)
                {

                    if (Curmycard[i] >= 2)
                    {
                        evaluate_prepare();
                        Curmycard[i] -= 2;
                        Cardnum -= 2;
                        double score = 0;
                        double temp = evaluate(0, 0, score);
                        temp += extra_point(2, i) - 0.05 + i / 200.0;
                        if (Cardnum == 0)
                            temp = -1e9;
                        Curmycard[i] += 2;
                        Cardnum += 2;
                        if (value > temp)
                        {
                            value = temp;
                            bestcard.clear();
                            pushcard(i, 2);
                        }
                    }
                }
            }

            //出连对
            {
                evaluate_prepare();
                int s = 0;
                int i = find_straight(s, 2);
                if (s >= 3)
                {
                    for (int j = 0; j < s; ++j)
                    {
                        Curmycard[i + j] -= 2;
                    }
                    Cardnum -= s * 2;
                    double score = 0;
                    double temp = evaluate(0, 0, score);
                    temp = temp - (15 - i) * 1.0 / 100 - 0.5;
                    if (Cardnum == 0) //清空
                        temp = -1e9;
                    for (int j = 0; j < s; ++j)
                    {
                        Curmycard[i + j] += 2;
                    }
                    Cardnum += s * 2;
                    if (value > temp)
                    {
                        value = temp;
                        bestcard.clear();
                        for (int t = 0; t < s; ++t)
                        {
                            pushcard(i + t, 2);
                        }
                    }
                }
            }

            //出三条，三带一，三带一对或者飞机
            {
                int s = 0;
                evaluate_prepare();
                int begin = 0;
                while (true)
                {
                    int i = find_straight(s, 3, begin);
                    //三条，存起来
                    if (s == 0)
                        break;
                    else
                    {
                        evaluate_prepare();
                        for (int j = 0; j < s; ++j)
                        {
                            Curmycard[i + j] -= 3;
                        }
                        Cardnum -= 3 * s;
                        int wings[15], pair = 0;
                        int wing[15], single = 0;
                        for (int t = 0; t < MAX_LEVEL && (t < i || t >= i + s); ++t)
                        {
                            if (Curmycard[t] == 1)
                            {
                                wing[single] = t;
                                single++;
                            }
                            else if (Curmycard[t] == 2)
                            {
                                wings[pair] = t;
                                pair++;
                            }
                        }

                        //飞机不带翼
                        double score = 0;
                        double temp = evaluate(0, 0, score);
                        if (i <= 7 && s >= 2)
                            temp = temp - 0.5 + extra_point(3, i); //i从小到大
                        else if (i <= 7)
                            temp = temp - 0.3 + extra_point(3, i);
                        else
                            temp -= 0.1 + extra_point(6, i);
                        if (Cardnum == 0) //清空
                            temp = -1e9;

                        if (value > temp)
                        {
                            value = temp;
                            bestcard.clear();
                            for (int t = i; t < i + s; ++t)
                            {
                                pushcard(t, 3);
                            }
                        }

                        //飞机带小翼
                        if (single >= s)
                        {
                            for (int o = 0; o < s; ++o)
                            {
                                Curmycard[wing[o]] = 0;
                            }
                            Cardnum -= s;
                            double score = 0;
                            double temp = evaluate(0, 0, score);
                            if (i <= 7 && s >= 2)
                                temp = temp - 0.5 + extra_point(3, i); //根据i的大小来出牌
                            else if (i <= 7)
                                temp = temp - 0.3 + extra_point(3, i);
                            else
                                temp -= 0.1 + extra_point(6, i);
                            if (Cardnum == 0) //一次清空
                                temp = -1e9;
                            for (int o = 0; o < s; ++o)
                            {
                                Curmycard[wing[o]] = 1;
                            }
                            Cardnum += s;
                            if (value > temp)
                            {
                                value = temp;
                                bestcard.clear();
                                for (int t = i; t < i + s; ++t)
                                {
                                    pushcard(t, 3);
                                }
                                for (int t = 0; t < s; ++t)
                                {
                                    pushcard(wing[t], 1);
                                }
                            }
                        }

                        //飞机带大翼
                        if (pair >= s)
                        {
                            for (int o = 0; o < s; ++o)
                            {
                                Curmycard[wings[o]] = 0;
                            }
                            Cardnum -= s * 2;
                            double score = 0;
                            double temp = evaluate(0, 0, score);
                            if (i <= 7 && s >= 2)
                                temp = temp - 0.5 + extra_point(3, i); //根据i的大小来出牌
                            else if (i <= 7)
                                temp = temp - 0.3 + extra_point(3, i);
                            else
                                temp -= 0.1 + extra_point(6, i);
                            if (Cardnum == 0) //能一下出完，太棒了！
                                temp = -1e9;
                            for (int o = 0; o < s; ++o)
                            {
                                Curmycard[wings[o]] = 2;
                            }
                            Cardnum += s * 2;
                            for (int j = 0; j < s; ++j)
                            {
                                Curmycard[i + j] += 3;
                            }
                            Cardnum += 3 * s;
                            if (value > temp)
                            {
                                value = temp;
                                bestcard.clear();
                                for (int t = i; t < i + s; ++t)
                                {
                                    pushcard(t, 3);
                                }
                                for (int t = 0; t < s; ++t)
                                {
                                    pushcard(wings[t], 2);
                                }
                            }
                        }
                    }
                    begin = i + s;
                }
            }

            //四带二
            {
                evaluate_prepare();
                for (int i = 0; i <= 12; ++i)
                {
                    if (Curmycard[i] == 4)
                    {
                        evaluate_prepare();
                        Curmycard[i] = 0;
                        Cardnum -= 4;
                        int _single[2], single = 0;
                        int _pair[2], pair = 0;
                        for (int t = 0; t < 12; ++t)
                        {
                            if (Curmycard[t] == 1 && single < 2)
                            {
                                _single[single] = t;
                                single++;
                            }
                            else if (Curmycard[t] == 2 && pair < 2)
                            {
                                _pair[pair] = t;
                                pair++;
                            }
                        }
                        //四带二单
                        if (single == 2)
                        {
                            Curmycard[_single[0]] = Curmycard[_single[1]] = 0;
                            Cardnum -= 2;
                            double score = 0;
                            double temp = evaluate(0, 0, score);
                            temp += extra_point(5, i);
                            if (Cardnum == 0) //能一下出完，太棒了！
                                temp = -1e9;
                            Curmycard[_single[0]] = Curmycard[_single[1]] = 1;
                            Cardnum += 2;
                            if (value > temp)
                            {
                                bestcard.clear();
                                value = temp;
                                for (int j = 0; j < 4; ++j)
                                    bestcard.push_back(Card(4 * i + j));
                                for (int j = 0; j < 2; ++j)
                                    pushcard(_single[j], 1);
                            }
                        }
                        //四带二对
                        if (pair == 2)
                        {
                            Curmycard[_pair[0]] = Curmycard[_pair[1]] = 0;
                            Cardnum -= 4;
                            double score = 0;
                            double temp = evaluate(0, 0, score);
                            temp += extra_point(6, i);
                            if (Cardnum == 0)
                                temp = -1e9;
                            Curmycard[_pair[0]] = Curmycard[_pair[1]] = 2;
                            Cardnum += 4;
                            if (value > temp)
                            {
                                bestcard.clear();
                                value = temp;
                                for (int j = 0; j < 4; ++j)
                                    bestcard.push_back(Card(4 * i + j));
                                for (int j = 0; j < 2; ++j)
                                    pushcard(_pair[j], 2);
                            }
                        }
                    }
                }
            }
        }
    }
flag:
    if (last_player == 2) //自己前一把打的牌没人要
    {
        return CardCombo(bestcard.begin(), bestcard.end());
    }
    else if (last_player == 1) //队友出的牌
    {
        if (value >= cur_score || cardRemaining[who_last_player] <= 3)
            return CardCombo();
        else
            return CardCombo(bestcard.begin(), bestcard.end());
    }
    else
    {
        if (myPosition == 0 && value - cur_score >= 4 && cardRemaining[who_last_player] >= 5)
            return CardCombo();
        else if (value - cur_score >= 3 && cardRemaining[who_last_player] > 5 && myPosition == 1)
            return CardCombo();
        else if (value - cur_score >= 4 && cardRemaining[who_last_player] > 5 && myPosition == 2)
            return CardCombo();
        else
            return CardCombo(bestcard.begin(), bestcard.end());
    }
}

namespace BotzoneIO
{
    using namespace std;
    void read()
    {
        // 读入输入（平台上的输入是单行）
        string line;
        getline(cin, line);
        Json::Value input;
        Json::Reader reader;
        reader.parse(line, input);

        // 首先处理第一回合，得知自己是谁、有哪些牌
        {
            auto firstRequest = input["requests"][0u]; // 下标需要是 unsigned，可以通过在数字后面加u来做到
            auto own = firstRequest["own"];
            for (unsigned i = 0; i < own.size(); i++)
                myCards.insert(own[i].asInt());
            if (!firstRequest["bid"].isNull())
            {
                // 如果还可以叫分，则记录叫分
                auto bidHistory = firstRequest["bid"];
                myPosition = bidHistory.size();
                for (unsigned i = 0; i < bidHistory.size(); i++)
                    bidInput.push_back(bidHistory[i].asInt());
            }
        }

        // history里第一项（上上家）和第二项（上家）分别是谁的决策
        int whoInHistory[] = {(myPosition - 2 + PLAYER_COUNT) % PLAYER_COUNT, (myPosition - 1 + PLAYER_COUNT) % PLAYER_COUNT};

        int turn = input["requests"].size();
        for (int i = 0; i < turn; i++)
        {
            auto request = input["requests"][i];
            auto llpublic = request["publiccard"];
            if (!llpublic.isNull())
            {
                // 第一次得知公共牌、地主叫分和地主是谁
                landlordPosition = request["landlord"].asInt();
                landlordBid = request["finalbid"].asInt();
                myPosition = request["pos"].asInt();
                whoInHistory[0] = (myPosition - 2 + PLAYER_COUNT) % PLAYER_COUNT;
                whoInHistory[1] = (myPosition - 1 + PLAYER_COUNT) % PLAYER_COUNT;
                cardRemaining[landlordPosition] += llpublic.size();
                for (unsigned i = 0; i < llpublic.size(); i++)
                {
                    landlordPublicCards.insert(llpublic[i].asInt());
                    if (landlordPosition == myPosition)
                        myCards.insert(llpublic[i].asInt());
                }
            }

            auto history = request["history"]; // 每个历史中有上家和上上家出的牌
            if (history.isNull())
                continue;
            stage = Stage::PLAYING;

            // 逐次恢复局面到当前
            int howManyPass = 0;
            for (int p = 0; p < 2; p++)
            {
                int player = whoInHistory[p];   // 是谁出的牌
                auto playerAction = history[p]; // 出的哪些牌
                vector<Card> playedCards;
                for (unsigned _ = 0; _ < playerAction.size(); _++) // 循环枚举这个人出的所有牌
                {
                    int card = playerAction[_].asInt(); // 这里是出的一张牌
                    playedCards.push_back(card);
                }
                whatTheyPlayed[player].push_back(playedCards); // 记录这段历史
                cardRemaining[player] -= playerAction.size();

                if (playerAction.size() == 0)
                    howManyPass++;
                else
                    lastValidCombo = CardCombo(playedCards.begin(), playedCards.end());
            }

            if (howManyPass == 2)
                lastValidCombo = CardCombo();

            if (i < turn - 1)
            {
                // 还要恢复自己曾经出过的牌
                auto playerAction = input["responses"][i]; // 出的哪些牌
                vector<Card> playedCards;
                for (unsigned _ = 0; _ < playerAction.size(); _++) // 循环枚举自己出的所有牌
                {
                    int card = playerAction[_].asInt(); // 这里是自己出的一张牌
                    myCards.erase(card);                // 从自己手牌中删掉
                    playedCards.push_back(card);
                }
                whatTheyPlayed[myPosition].push_back(playedCards); // 记录这段历史
                cardRemaining[myPosition] -= playerAction.size();
            }
        }
    }

    /**
	* 输出叫分（0, 1, 2, 3 四种之一）
	*/
    void bid(int value)
    {
        Json::Value result;
        result["response"] = value;

        Json::FastWriter writer;
        cout << writer.write(result) << endl;
    }

    /**
	* 输出打牌决策，begin是迭代器起点，end是迭代器终点
	* CARD_ITERATOR是Card（即short）类型的迭代器
	*/
    template <typename CARD_ITERATOR>
    void play(CARD_ITERATOR begin, CARD_ITERATOR end)
    {
        Json::Value result, response(Json::arrayValue);
        for (; begin != end; begin++)
            response.append(*begin);
        result["response"] = response;

        Json::FastWriter writer;
        cout << writer.write(result) << endl;
    }
}
int callbid(int before)
{
    double card = 0;
    int value_origin = evaluate(0, 0, card);
    if (value_origin <= -2)
    {
        if (before < 3)
            return 3;
        else if (before < 2)
            return 2;
        else if (before < 1)
            return 1;
        return 0;
    }
    else if (value_origin > -2 && value_origin < -1)
    {
        if (before < 2)
            return 2;
        else if (before < 1)
            return 1;
        else
            return 0;
    }
    else if (value_origin >= -1 && value_origin < 0.5)
    {
        if (before < 1)
            return 1;
        else
            return 0;
    }
    else
        return 0;
}
void insert_all_card()
{
    for (int i = 0; i < 53; ++i)
        remain_card.insert(Card(i));
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < whatTheyPlayed[i].size(); ++j)
            for (int k = 0; k < whatTheyPlayed[i][j].size(); ++k)
            {
                remain_card.erase(whatTheyPlayed[i][j][k]);
            }
    }
    for (auto i = myCards.begin(); i != myCards.end(); ++i)
    {
        remain_card.erase(*i);
    }
    return;
}

int main()
{
    srand(time(nullptr));
    BotzoneIO::read();

    if (stage == Stage::BIDDING)
    {
        // 做出决策（你只需修改以下部分）

        auto maxBidIt = std::max_element(bidInput.begin(), bidInput.end());
        int maxBid = maxBidIt == bidInput.end() ? -1 : *maxBidIt;

        //int bidValue = rand() % (3 - maxBid) + maxBid + 1;
        int bidValue = callbid(maxBid);
        // 决策结束，输出结果（你只需修改以上部分）

        BotzoneIO::bid(bidValue);
    }
    else if (stage == Stage::PLAYING)
    {
        // 做出决策（你只需修改以下部分）

        // findFirstValid 函数可以用作修改的起点
        insert_all_card();
        // 是合法牌
        CardCombo myAction = CardCombo();

        myAction = findBestValid();
        // 是合法牌
        assert(myAction.comboType != CardComboType::INVALID);

        assert(
            // 在上家没过牌的时候过牌
            (lastValidCombo.comboType != CardComboType::PASS && myAction.comboType == CardComboType::PASS) ||
            // 在上家没过牌的时候出打得过的牌
            (lastValidCombo.comboType != CardComboType::PASS && lastValidCombo.canBeBeatenBy(myAction)) ||
            // 在上家过牌的时候出合法牌
            (lastValidCombo.comboType == CardComboType::PASS && myAction.comboType != CardComboType::INVALID));

        // 决策结束，输出结果（你只需修改以上部分）

        BotzoneIO::play(myAction.cards.begin(), myAction.cards.end());
    }
}
/*
int main()
{
    BotzoneIO::input();
    insert_all_card();
    // 是合法牌
    CardCombo myAction = CardCombo();
    try
    {
        myAction = findBestValld();
        assert(myAction.comboType != CardComboType::INVALID);

        assert(
            // 在上家没过牌的时候过牌
            (lastValidCombo.comboType != CardComboType::PASS && myAction.comboType == CardComboType::PASS) ||
            // 在上家没过牌的时候出打得过的牌
            (lastValidCombo.comboType != CardComboType::PASS && lastValidCombo.canBeBeatenBy(myAction)) ||
            // 在上家过牌的时候出合法牌
            (lastValidCombo.comboType == CardComboType::PASS && myAction.comboType != CardComboType::INVALID));
    }
    catch (exception &e)
    {
        myAction = lastValidCombo.findFirstValid(myCards.begin(), myCards.end());
    }

    决策结束，输出结果（你只需修改以上部分）

    BotzoneIO::output(myAction.cards.begin(), myAction.cards.end());
}
*/
//需要加入特判：主动时如果最后两手牌要先打高级牌