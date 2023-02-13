#include <iostream>
#include <set>
#include <string>
#include <cassert>
#include <cstring> // 注意memset是cstring里的
#include <algorithm>
#include "jsoncpp/json.h" // 在平台上，C++编译时默认包含此库

using std::set;
using std::sort;
using std::string;
using std::unique;
using std::vector;

constexpr int PLAYER_COUNT = 3;

enum class Stage
{
	BIDDING, // 叫分阶段
	PLAYING	 // 打牌阶段
};

enum class CardComboType
{
	PASS,		// 过
	SINGLE,		// 单张
	PAIR,		// 对子
	STRAIGHT,	// 顺子
	STRAIGHT2,	// 双顺
	TRIPLET,	// 三条
	TRIPLET1,	// 三带一
	TRIPLET2,	// 三带二
	BOMB,		// 炸弹
	QUADRUPLE2, // 四带二（只）
	QUADRUPLE4, // 四带二（对）
	PLANE,		// 飞机
	PLANE1,		// 飞机带小翼
	PLANE2,		// 飞机带大翼
	SSHUTTLE,	// 航天飞机
	SSHUTTLE2,	// 航天飞机带小翼
	SSHUTTLE4,	// 航天飞机带大翼
	ROCKET,		// 火箭
	INVALID		// 非法牌型
};

int cardComboScores[] = {
	0,	// 过
	1,	// 单张
	2,	// 对子
	6,	// 顺子
	6,	// 双顺
	4,	// 三条
	4,	// 三带一
	4,	// 三带二
	10, // 炸弹
	8,	// 四带二（只）
	8,	// 四带二（对）
	8,	// 飞机
	8,	// 飞机带小翼
	8,	// 飞机带大翼
	10, // 航天飞机（需要特判：二连为10分，多连为20分）
	10, // 航天飞机带小翼
	10, // 航天飞机带大翼
	16, // 火箭
	0	// 非法牌型
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
	vector<Card> cards;		 // 原始的牌，未排序
	vector<CardPack> packs;	 // 按数目和大小排序的牌种
	CardComboType comboType; // 算出的牌型
	Level comboLevel = 0;	 // 算出的大小序

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

/* 状态 */

// 我的牌有哪些
set<Card> myCards;

// 地主明示的牌有哪些
set<Card> landlordPublicCards;

// 大家从最开始到现在都出过什么
vector<vector<Card>> whatTheyPlayed[PLAYER_COUNT];

// 当前要出的牌需要大过谁
CardCombo lastValidCombo;

// 大家还剩多少牌
short cardRemaining[PLAYER_COUNT] = {17, 17, 17};

// 我是几号玩家（0-地主，1-农民甲，2-农民乙）
int myPosition;

// 地主位置
int landlordPosition = -1;

// 地主叫分
int landlordBid = -1;

// 阶段
Stage stage = Stage::BIDDING;

// 自己的第一回合收到的叫分决策
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
int  callbid(int before)
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
	//if (lastValidCombo.comboType == CardComboType::ROCKET) //别人如果王炸就过掉
		//return CardCombo();

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
	//以上是标准参考评估代码，下面是自己写的出牌决策
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
			int c, s;																																							  //分别是coumputer和science[/huaji]
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
		//主动出牌

		evaluate_prepare();
		for (int i = 0; i < 7; i++)
			if (Curmycard[i] == 3 && Curmycard[i + 1] == 3) //飞机优先级最高。此处i+1到10，即飞机的搜索从3到10。
			{
				int counting[2] = {0}; //记录单牌与对子
				for (int j = 0; j <= 7; j++)
				{
					if (Curmycard[j] == 1)
						counting[0]++;
					if (Curmycard[j] == 2)
						counting[1]++;
				}
				if (counting[1] >= counting[0] && counting[1] >= 2)
				{
					for (int j = 0; j <= 8; j++)
					{
						int ss = 0;
						if (Curmycard[j] == 1)
						{
							bestcard.clear();
							pushcard(j, 2);

							ss++;
						}
						if (ss == 2)
							break;
					}
				}
				else if (counting[0] >= counting[1] && counting[0] >= 2)
				{
					bestcard.clear();
					for (int j = 0; j <= 8; j++)
					{
						int ss = 0;
						if (Curmycard[j] == 1)
						{
							pushcard(j, 1);
							ss++;
						}
						if (ss == 2)
							break;
					}
				}
				for (int j = 1; j <= 3; j++)
				{
					pushcard(i, 1);
					pushcard(i + 1, 1);
				}
				//对于带，搜索对与单，跳过连对与顺子
				continue;
			}
		int tip[2][5] = {0};		 //记录双顺出现的特殊情况，tip[0]代表类型，0为三带，1为顺子，0为无，1为有。其余记录情况,0记录三带的情况，1记录顺子的情况。
		for (int i = 0; i <= 8; i++) //对于顺子，进行寻找。这里单双顺子都寻找。先双后单
		{
			if ((Curmycard[i] >= 2 && Curmycard[i] <= 3) && (Curmycard[i + 1] >= 2 && Curmycard[i + 1] <= 3) && (Curmycard[i + 2] >= 2 && Curmycard[i + 2] <= 3)) //先双顺后单顺，在jjQQKK停止,不要拆炸弹
			{
				int keeping = 0; //表示后续长度
				for (int j = i + 3; j <= 14; j++)
				{
					if ((Curmycard[j] >= 2 && Curmycard[j] != 4)) //注意不要拆弹
					{
						keeping++;
					}
					else if (Curmycard[j] < 2)
						break;
				} //首先确定后续是否有双顺，在KK处停止
				int three = 0;
				for (int j = i; j <= j + 3 + keeping; j++)
				{
					if (Curmycard[j] == 3)
					{
						three++;
					}
				} //  确定是否有三带,当三带数超过序列长度一半时，选择三带出牌。
				if (three * 2 >= 3 + keeping)
				{
					tip[0][0] = 1;
					break;
				}
				int single = 0;
				if (keeping >= 2) //当双联为5张时选择，看是否有顺子可以出
				{
					int before = 0;
					for (;; before++)
						if (Curmycard[i - before] == 0 || Curmycard[i - before] == 4)
							break; //如果有前面单牌，就连上,注意不要拆炸弹
					int after = 0;
					for (;; after++)
						if ((Curmycard[i + keeping + 3 + after] == 0 || Curmycard[i + keeping + 3 + after] == 4) && i + keeping + 3 + after < 15)
							break; //如果后面有单牌，就连上，跳转至顺子出牌，即跳出连对搜索
					if (before != 0 || after != 0)
					{
						tip[1][0] = 1;
						tip[1][1] = i;
						tip[1][2] = 3 + keeping;
						tip[1][3] = before;
						tip[1][4] = after;
						break;
					}
				}
				else if (keeping == 1) //当双联为4张时选择，看是否有顺子可以出
				{
					if (Curmycard[i - 1] == 1 && Curmycard[i + 1] == 1 && i + 1 < 15)
					{
						int before = 0;
						for (;; before++)
							if (Curmycard[i - before] == 0 || Curmycard[i - before] == 4)
								break; //如果有前面单牌，就连上,注意不要拆炸弹
						int after = 0;
						for (;; after++)
							if ((Curmycard[i + keeping + 3 + after] == 0 || Curmycard[i + keeping + 3 + after] == 4) && i + keeping + 3 + after < 15)
								break; //如果后面有单牌，就连上，跳转至顺子出牌，即跳出连对搜索
						tip[1][0] = 1;
						tip[1][1] = i;
						tip[1][2] = 3 + keeping;
						tip[1][3] = before;
						tip[1][4] = after;
						break;
					}
				}
				else if (keeping == 0) //当双联为3张时选择，看是否有顺子可以出
				{
					if (Curmycard[i - 1] == 1 && Curmycard[i + 1] == 1 && Curmycard[i - 2] == 1 && Curmycard[i + 2] == 1)
					{
						int before = 0;
						for (;; before++)
							if (Curmycard[i - before] == 0 || Curmycard[i - before] == 4)
								break; //如果有前面单牌，就连上,注意不要拆炸弹
						int after = 0;
						for (;; after++)
							if ((Curmycard[i + keeping + 3 + after] == 0 || Curmycard[i + keeping + 3 + after] == 4) && i + keeping + 3 + after <= 14)
							{ //如果后面有单牌，就连上，跳转至顺子出牌，即跳出连对搜索
								tip[1][0] = 1;
								tip[1][1] = i;
								tip[1][2] = 3 + keeping;
								tip[1][3] = before;
								tip[1][4] = after;
								break;
							}
					}
				}
				//当以上均没有时，可以输出
				bestcard.clear();
				for (int j = i; j <= i + 3 + keeping; j++)
				{
					pushcard(j, 1);
					pushcard(j, 1);
				}
				goto flag;
				//确定后续长度，进行输出
			}
		}

		if (tip[1][0] == 1) //确定是否有双连续的顺子，如果有，选择顺子出牌
		{
			for (int j = tip[1][1] - tip[1][3]; j <= tip[1][1] + tip[1][2] + tip[1][4]; j++)
			{
				pushcard(j, 1);
			}
			goto flag;
		}
		for (int i = 0; i <= 8; i++)																																																									  //单顺,到8910jq
			if ((Curmycard[i - 1] >= 1 && Curmycard[i - 1] != 4) && (Curmycard[i + 1] >= 1 && Curmycard[i + 1] != 4) && (Curmycard[i + 2] >= 1 && Curmycard[i + 2] != 4) && (Curmycard[i + 3] >= 1 && Curmycard[i + 3] != 4) && (Curmycard[i] >= 1 && Curmycard[i] != 4)) //单顺，在8910jq结束
			{
				int after = 1;
				for (;; after++)
				{
					if ((Curmycard[i + 3 + after] == 1 || Curmycard[i + 3 + after] == 2) && i + 3 + after < 12)
						;
					else
						break;
				}
				for (int j = i - 1; j <= i + 3 + after; j++)
				{
					pushcard(j, 1);
				}
				goto flag;
			}
		//三带。此时顺子、飞机已经没有了，直接暴力搜索,到J截止
		for (int i = 0; i <= 8; i++)
		{
			if (Curmycard[i] == 3)
			{
				int counting[2] = {0}; //记录单牌与对子
				for (int j = 0; j <= 8; j++)
				{
					if (Curmycard[j] == 1)
						counting[0]++;
					if (Curmycard[j] == 2)
						counting[1]++;
				}
				if (counting[1] >= counting[0])
				{
					for (int j = 3; j <= 11; j++)
					{
						if (Curmycard[j] == 1)
						{
							pushcard(j, 1);
							pushcard(j, 1);
							break;
						}
					}
				}
				else if (counting[0] >= counting[1])
				{
					for (int j = 0; j <= 8; j++)
					{
						if (Curmycard[j] == 1)
						{
							pushcard(j, 1);
							break;
						}
					}
				}
				for (int j = 1; j <= 3; j++)
				{
					pushcard(i, 1);
					goto flag;
				}
			}
		}
		//单牌、对子,出牌至2
		{
			int counting[2] = {0};
			for (int i = 12; i <= 17; i++)
			{
				counting[0] += Curmycard[i];
				counting[1] = counting[1] + Curmycard[i] / 2;
				if (Curmycard[i] == 4)
				{
					counting[0] = counting[0] - 4;
					counting[1] = counting[1] - 2;
				}
			}
			if (Curmycard[14] == 1 || Curmycard[15] == 1)
			{
				counting[0] = counting[0] - 2;
			}
			if (counting[0] >= counting[1])
			{
				for (int i = 0; i <= 12; i++)
				{
					if (Curmycard[i] == 1)
					{
						pushcard(i, 1);
						goto flag;
					}
				}
			}
			else if (counting[0] <= counting[1])
			{
				for (int i = 0; i <= 12; i++)
				{
					if (Curmycard[i] == 2)
					{
						pushcard(i, 1);
						pushcard(i, 1);
						goto flag;
					}
				}
			}
		}
		//最后，极其特殊的情况
		//qqqkkk或kkkAAA，这种情况没必要用飞机，直接采用三带。此时，已经没有小于等于2的单牌或对子、没有小于等于J的三带。此时只剩QQQKKKAAA222小王大王和炸弹
		//首先，搜索三条
		for (int i = 9; i <= 12; i++)
		{
			if (Curmycard[i] == 3)
			{
				pushcard(i, 1);
				pushcard(i, 1);
				pushcard(i, 1);
				goto flag;
			}
		}
		//然后是单个王
		{
			if ((Curmycard[13] == 0 && Curmycard[14] == 1) && (Curmycard[13] == 1 && Curmycard[14] == 0))
			{
				if (Curmycard[13] == 0)
				{
					pushcard(13, 1);
					goto flag;
				}
				else if (Curmycard[14] == 1)
				{
					pushcard(14, 1);
					goto flag;
				}
			}
		}
		//其次，搜索卡布奇诺
		for (int i = 0; i <= 12; i++)
		{

			{
				if (Curmycard[i] == 4)
				{
					pushcard(i, 1);
					pushcard(i, 1);
					pushcard(i, 1);
					pushcard(i, 1);
					goto flag;
				}
			}
		}
		//最后，搜索王炸
		if (Curmycard[13] == 1 && Curmycard[14] == 1)
		{
			pushcard(13, 1);
			pushcard(14, 1);
			goto flag;
		}
		//结束
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
				int player = whoInHistory[p];	// 是谁出的牌
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
					myCards.erase(card);				// 从自己手牌中删掉
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