#include <iostream> //注意千万要记得归位或者直接不要动！！！
#include <string>   //判断是否两手牌,也能够判断一手牌
#include <vector>
#include <time.h>
using namespace std;    //感觉还是比较亏，这个时间七四可以直接用估值函数里面判断，不过在主动出牌应该还是好用的
int Curmycard_copy[15]; //觉得可能最好还能返回两手牌（或者一手牌）的牌型和优先级
int cardnum_copy;
vector<int> bestcard;
void push_back(int item, int num = 1)
{
    for (int i = 1; i <= num; i++)
    {
        bestcard.push_back(item);
    }
}
string judge()
{
    bool best = false;
    int count = 0;
    //先王炸
    if (Curmycard_copy[13] == 1 && Curmycard_copy[14] == 1)
    {
        best = true;
        Curmycard_copy[13] = 0;
        Curmycard_copy[14] = 0;
        cardnum_copy -= 2;
        count++;
        bestcard.clear(); //需要clear，因为push_back里面不会去清空
        push_back(13);
        push_back(14);
    }
    //炸弹
    for (int i = 0; i <= 12; i++)
    {
        if (Curmycard_copy[i] == 4)
        {
            Curmycard_copy[i] -= 4;
            count++;
            cardnum_copy -= 4;
            if (best != true)
            {
                best = true;
                bestcard.clear();
                push_back(i, 4);
            }
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
                if (!best)
                {
                    best = true; //把最后两步写完就可以调估值函数力
                    bestcard.clear();
                    for (int j = i - chain; j < i; j++)
                    {
                        push_back(j, 2);
                    }
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

        for (vector<int>::iterator i = bestcard.begin(); i != bestcard.end(); i++)
            cout << *i << " ";
        return "yeah";
    }
    if (count > 2)
        return "shit";
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
                if (!best)
                {
                    best = true; //把最后两步写完就可以调估值函数力
                    bestcard.clear();
                    for (int j = i - chain; j < i; j++)
                    {
                        push_back(j, 1);
                    }
                }
                chain = 0;
            }
        }
    }
    if (count <= 2 && cardnum_copy == 0)
    {

        for (vector<int>::iterator i = bestcard.begin(); i != bestcard.end(); i++)
            cout << *i << " ";
        return "yeah";
    }
    if (count > 2)
        return "shit";
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
                if (!best)
                {
                    best = true;
                    bestcard.clear();
                    push_back(i, tmp);
                    push_back(three, 3);
                }
                break;
            }
        }
    }
    if (count <= 2 && cardnum_copy == 0)
    {

        for (vector<int>::iterator i = bestcard.begin(); i != bestcard.end(); i++)
            cout << *i << " ";
        return "yeah";
    }
    if (count > 2)
        return "shit";
    //找对子
    for (int i = 0; i <= 12; i++)
    {
        if (Curmycard_copy[i] == 2)
        {
            Curmycard_copy[i] = 0;
            cardnum_copy -= 2;
            count++;
            if (!best)
            {
                best = true; //把最后两步写完就可以调估值函数力
                bestcard.clear();
                push_back(i, 2);
            }
            break;
        }
    }
    if (count <= 2 && cardnum_copy == 0)
    {

        for (vector<int>::iterator i = bestcard.begin(); i != bestcard.end(); i++)
            cout << *i << " ";
        return "yeah";
    }
    if (count > 2)
        return "shit";
    //最后，单根
    for (int i = 0; i <= 14; i++)
    {
        if (Curmycard_copy[i] == 1)
        {
            Curmycard_copy[i] = 0;
            cardnum_copy--;
            count++;
            if (!best)
            {
                push_back(i);
            }
            if (count > 2)
                break;
        }
    }
    if (count <= 2 && cardnum_copy == 0)
    {

        for (vector<int>::iterator i = bestcard.begin(); i != bestcard.end(); i++)
            cout << *i << " ";
        return "yeah";
    }
    else
        return "shit";
}
int main()
{
    clock_t begin_time, end_time;
    begin_time = clock();
    while (cin >> cardnum_copy)
    {
        if (cardnum_copy == 0)
            break;
        int now;

        int count_input = 0;
        while (cin >> now)
        {
            count_input++;

            Curmycard_copy[now]++;
            if (count_input >= cardnum_copy)
            {
                cout << judge() << endl;
                for (int i = 0; i <= 14; i++)
                    Curmycard_copy[i] = 0;

                break;
            }
        }
        end_time = clock();
        cout << end_time - begin_time << endl;
    }

    system("pause");
    return 0;
}

/*
//bool插件版本
bool judge_last_two()
{

    int Curmycard_copy[15];
    int cardnum_copy;
    for (int i = 0; i <= 14; i++)
        Curmycard_copy[i] = Curmycard[i];
    cardnum_copy = cardnum;


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

            break;
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
                break;
        }
    }
    if (count <= 2 && cardnum_copy == 0)
    {

        return true;
    }
    else
        return false;
}
*/