using namespace std;
#include<algorithm>
#include<stdio.h>
#include<stdlib.h>
#include<math.h>
#include<map>
#include<set>
#include<tuple>
#include<iostream>
#include<string>
#include<array>
#include<vector>
#include<iterator>
#include<list>

/* 
$ g++ -O3 -fopenmp blackjack.cpp
$ time ./a.out 
*/

typedef array<int, 11> Deck;
map<Deck, int> indexer;
map<Deck, int> cache;
int deck_number = 1;
int **deck_mapping; // deck_mapping[d][c] = deck index obtained from drawing card c from deck d
int **decks; // decks[d][c] = number of cards = c in deck d


// These quantities pertain to the calculation of dealer probabilities
map<Deck, int> composition_index;
int **composition_counts; // indexed by first card and  composition number
int **composition_values; // indexed by first card and composition number
int comp_number = 1;
int **composition_cards; // set of unique hands
list<int> **composition_lists; // indexed by first card and final value

double ***cache_dealer; // deck number, upcard (A-10), final value (17-22)
double ***cache_stand; // indexed by deck_number, dealer upcard (A-10), total (16-21)
double ***cache_hit_hard;
double ***cache_hit_soft;
double ***cache_double_hard;
double ***cache_double_soft;
double ***cache_split;
double ev_hit_soft(int deck, int total, int upcard);

void initialize() {
    int num_decks = 35946;
    deck_mapping = new int*[num_decks];
    decks = new int*[num_decks];
    for(int i=0; i < num_decks; i++) {
        deck_mapping[i] = new int[11];  
        decks[i] = new int[11];
    }

    int comps = 5000;
    composition_counts = new int*[11];
    composition_values = new int*[11];
    composition_cards = new int*[comps];
    composition_lists = new list<int>*[11];
    for(int i=0; i < 11; i++) { 
        composition_counts[i] = new int[comps];
        composition_values[i] = new int[comps];
        composition_lists[i] = new list<int>[6];
    }
    for(int i=0; i < comps; i++)
        composition_cards[i] = new int[11];

    cache_dealer = new double**[num_decks];
    cache_stand = new double**[num_decks];
    cache_hit_hard = new double**[num_decks];
    cache_hit_soft = new double**[num_decks];
    cache_double_hard = new double**[num_decks];
    cache_double_soft = new double**[num_decks];
    cache_split = new double**[num_decks];
    for(int i=0; i < num_decks; i++) {
        cache_dealer[i] = new double*[10];
        cache_stand[i] = new double*[10];
        cache_hit_hard[i] = new double*[10];
        cache_hit_soft[i] = new double*[10];
        cache_double_hard[i] = new double*[10];
        cache_double_soft[i] = new double*[10];
        cache_split[i] = new double*[10];
        for(int j=0; j < 10; j++) {
            cache_dealer[i][j] = new double[6];
            cache_stand[i][j] = new double[6];
            cache_hit_hard[i][j] = new double[18];
            cache_hit_soft[i][j] = new double[10];
            cache_double_hard[i][j] = new double[18];
            cache_double_soft[i][j] = new double[10];
            cache_split[i][j] = new double[10];
        }
    }

}

Deck add(Deck d, int c) {
    Deck d2;
    for(int i=0; i < 11; i++) d2[i] = d[i];
    d2[0]++; d2[c]++;
    return d2;
}

Deck draw(Deck d, int c) {
    Deck d2;
    for(int i=0; i < 11; i++) d2[i] = d[i];
    d2[0]--; d2[c]--;
    return d2;
}


int compute_dealer_sequences(int first, Deck cards, int total, bool soft = false) { 
    if(total > 21 && soft) { 
        return compute_dealer_sequences(first, cards, total - 10, false);
    }
    else if(total > 17 || (total == 17 && !soft)) {

        if(composition_index[cards] == 0) {
            composition_index[cards] = comp_number++;
            for(int i=0; i < 11; i++)
                composition_cards[composition_index[cards]][i] = cards[i]; 
        }
        composition_values[first][composition_index[cards]] = min(total, 22);
        composition_counts[first][composition_index[cards]]++;
        return 1;
    }
    else {
        int ans = 0;
        for(int c=1; c < 11; c++) {
            Deck c2 = add(cards, c);
            if(soft && c==1)
                ans += compute_dealer_sequences(first, c2, total+1, true);
            else if(c==1)
                ans += compute_dealer_sequences(first, c2, total+11, true);
            else
                ans += compute_dealer_sequences(first, c2, total+c, soft);
        }
        return ans;
    }
}

int dealer_sequences() {
    Deck cards = {0,0,0,0,0,0,0,0,0,0,0};
    int ans = 0;
    for(int c=1; c < 11; c++) { 
        int total = c != 1 ? c : 11;
        ans += compute_dealer_sequences(c, cards, total, c==1);
    }

    for(int c=1; c < 11; c++) {
        for(int comp=1; comp < 3157; comp++) {
            int count = composition_counts[c][comp];
            if(count > 0) {
                int total = composition_values[c][comp];
                //cout << count << "\t" << total << endl;
                composition_lists[c][total-17].push_front(comp);
            }
        }
    }

    return ans;
}

double dealer_natural_probability(int deck, int upcard) {
    auto d = decks[deck];
    if(upcard == 1)
        return (double) d[10] / d[0];
    else if(upcard == 10) 
        return (double) d[1] / d[0];
    else 
        return 0.0;
}

double dealer_conditional_probability(int deck, int upcard, int value) {
    /*** Compute the conditional probability of dealer ending with a 
        value (17-22) given the upcard (A-10) and the deck.  
    ***/
    if(cache_dealer[deck][upcard-1][value-17] != 0)
        return cache_dealer[deck][upcard-1][value-17];
    auto d = decks[deck];
    double bj = dealer_natural_probability(deck, upcard);
    double ans = 0.0;
    if(value == 21)
        ans -= bj;
    for(int comp : composition_lists[upcard][value-17]) {
        int *cards = composition_cards[comp];
        double proba = composition_counts[upcard][comp];
        double denom = d[0];
        bool valid = true;
        for(int i=1; i < 11; i++)
            if(d[i] >= cards[i]) {
                for(int j=0; j < cards[i]; j++) { 
                    proba *= (d[i] - j) / denom;
                    denom -= 1.0;
                }
            } else {
                valid = false;
                break;
            }
        if(valid) {
            ans += proba;
        }
    }
    ans /= (1 - bj);
    cache_dealer[deck][upcard-1][value-17] = ans;
    return ans;
}

double ev_stand(int deck, int total, int upcard) {
    if(cache_stand[deck][upcard-1][max(total-16,0)] != 0)
        return cache_stand[deck][upcard-1][max(total-16,0)];
    double p17 = dealer_conditional_probability(deck, upcard, 17);
    double p18 = dealer_conditional_probability(deck, upcard, 18);
    double p19 = dealer_conditional_probability(deck, upcard, 19);
    double p20 = dealer_conditional_probability(deck, upcard, 20);
    double p21 = dealer_conditional_probability(deck, upcard, 21);
    double p22 = dealer_conditional_probability(deck, upcard, 22);
    double ev;
    if(total < 17)  ev = p22 - p17 - p18 - p19 - p20 - p21;
    if(total == 17) ev = p22 - p18 - p19 - p20 - p21;
    if(total == 18) ev = p22 + p17 - p19 - p20 - p21;
    if(total == 19) ev = p22 + p17 + p18 - p20 - p21;
    if(total == 20) ev = p22 + p17 + p18 + p19 - p21;
    if(total == 21) ev = p22 + p17 + p18 + p19 + p20;
    cache_stand[deck][upcard-1][max(total-16,0)] = ev;
    return ev;
}

double ev_hit_hard(int deck, int total, int upcard) {
    if(cache_hit_hard[deck][upcard-1][total-4] != 0)
        return cache_hit_hard[deck][upcard-1][total-4];
    auto d = decks[deck];
    double ev = 0.0;
    for(int c=1; c < 11; c++)
        if(d[c] > 0) {
            int deck2 = deck_mapping[deck][c];
            double p = (double) d[c] / d[0];
            if(c == 1 && total + 11 <= 21) { 
                double H = ev_hit_soft(deck2, total+11, upcard);
                double S = ev_stand(deck2, total+11, upcard);
                ev += p*max(H,S);
            } else if(total + c <= 21) {
                double H = ev_hit_hard(deck2, total+c, upcard);
                double S = ev_stand(deck2, total+c, upcard);
                ev += p*max(H,S);
            } else {
                ev -= p;
            }
        }
    cache_hit_hard[deck][upcard-1][total-4] = ev;
    return ev;        
}

double ev_hit_soft(int deck, int total, int upcard) { 
    if(cache_hit_soft[deck][upcard-1][total-12] != 0)
        return cache_hit_soft[deck][upcard-1][total-12];
    auto d = decks[deck];   
    double ev = 0.0;
    for(int c=1; c < 11; c++)
        if(d[c] > 0) { 
            double p = (double) d[c] / d[0];
            int deck2 = deck_mapping[deck][c];
            if(total + c <= 21) { 
                double H = ev_hit_soft(deck2, total+c, upcard);
                double S = ev_stand(deck2, total+c, upcard);
                ev += p*max(H,S);
            } else {
                double H = ev_hit_hard(deck2, total+c-10, upcard);
                double S = ev_stand(deck2, total+c-10, upcard);
                ev += p*max(H,S);
            }
        }
    return cache_hit_soft[deck][upcard-1][total-12] = ev;
    return ev;
}

double ev_double_hard(int deck, int total, int upcard) {
    if(cache_double_hard[deck][upcard-1][total-4] != 0)
        return cache_double_hard[deck][upcard-1][total-4];
    auto d = decks[deck];
    double ev = 0.0;
    for(int c=1; c < 11; c++)
        if(d[c] > 0) {
            int deck2 = deck_mapping[deck][c];
            double p = (double) d[c] / d[0];
            if(c == 1 && total + 11 <= 21) { 
                double S = ev_stand(deck2, total+11, upcard);
                ev += p*S;
            } else if(total + c <= 21) {
                double S = ev_stand(deck2, total+c, upcard);
                ev += p*S;
            } else {
                ev -= p;
            }
        }
    cache_double_hard[deck][upcard-1][total-4] = 2*ev;
    return 2*ev;        
}

double ev_double_soft(int deck, int total, int upcard) { 
    if(cache_double_soft[deck][upcard-1][total-12] != 0)
        return cache_double_soft[deck][upcard-1][total-12];
    auto d = decks[deck];   
    double ev = 0.0;
    for(int c=1; c < 11; c++)
        if(d[c] > 0) { 
            double p = (double) d[c] / d[0];
            int deck2 = deck_mapping[deck][c];
            if(total + c <= 21) { 
                double S = ev_stand(deck2, total+c, upcard);
                ev += p*S;
            } else {
                double S = ev_stand(deck2, total+c-10, upcard);
                ev += p*S;
            }
        }
    return cache_double_soft[deck][upcard-1][total-12] = 2*ev;
    return 2*ev;
}

double ev_split(int deck, int card, int upcard) { 
    if(cache_split[deck][upcard-1][card-1] != 0)
        return cache_split[deck][upcard-1][card-1];
    auto d = decks[deck];
    double ev = 0.0;
    for(int c=1; c < 11; c++)
        if(d[c] > 0) {
            double p = (double) d[c] / d[0];
            int deck2 = deck_mapping[deck][c];
            if(card == 1) {
                double S = ev_stand(deck2, 11+c, upcard);
                ev += p*S;
            } else if(c == 1) {
                double S = ev_stand(deck2, 11+card, upcard);
                double H = ev_hit_soft(deck2, 11+card, upcard);
                double D = ev_double_soft(deck2, 11+card, upcard);
                ev += p*max(S,max(H,D));
            } else {
                double S = ev_stand(deck2, card+c, upcard);
                double H = ev_hit_hard(deck2, card+c, upcard);
                double D = ev_double_hard(deck2, card+c, upcard);
                ev += p*max(S,max(H,D));
            }
        }
    cache_split[deck][upcard-1][card-1] = 2*ev;
    return 2*ev;
}

double compute_ev(int deck, int card1, int card2, int card3) {
    auto d = decks[deck];
    double p4 = 0.0;
    if(card3 == 1) p4 = (double) d[10] / d[0];
    if(card3 == 10) p4 = (double) d[1] / d[0];
    // we have natural blackjack
    if((card1 == 1 && card2 == 10) || (card1 == 10 && card2 == 1)) {
        return (1-p4)*1.5 + p4*0.0;
    }
    // we do not have blackjack
    double H = -2.0, S = -2.0, P = -2.0, D = -2.0;
    double R = -0.5;
    R = -2.0; // No Surrender
    if(card1 == card2) {
        if(card1 == 1) {
            H = ev_hit_soft(deck, 12, card3);
            S = ev_stand(deck, 12, card3);
            P = ev_split(deck, 1, card3);
            D = ev_double_soft(deck, 12, card3);
        } else { 
            H = ev_hit_hard(deck, card1*2, card3);
            S = ev_stand(deck, card1*2, card3);
            P = ev_split(deck, card1, card3);
            D = ev_double_hard(deck, card1*2, card3);  
        }
    } else if(card1 == 1 || card2 == 1) {
        H = ev_hit_soft(deck, card1+card2+10, card3);
        S = ev_stand(deck, card1+card2+10, card3);
        D = ev_double_soft(deck, card1+card2+10, card3);
    } else {
        H = ev_hit_hard(deck, card1+card2, card3);
        S = ev_stand(deck, card1+card2, card3);
        D = ev_double_hard(deck, card1+card2, card3);
    }
    double best = max(H, max(S, max(P, max(D, R))));
    return (1-p4)*best - 1.0*p4;
}

double expected_value_of_game(Deck deck) { 
    double ev = 0.0;
    #pragma omp parallel for reduction(+:ev)
    for(int i=1; i < 11; i++) {
        if(deck[i] > 0) { 
            double p1 = (double) deck[i] / deck[0];
            Deck deck1 = draw(deck, i);
            for(int j=i; j < 11; j++) {
                if(deck1[j] > 0) { 
                    double p2 = (double) deck1[j] / deck1[0];
                    if(i != j) p2 *= 2.0;
                    Deck deck2 = draw(deck1, j);
                    for(int k=1; k < 11; k++) {
                        if(deck2[k] > 0) { 
                            double p3 = (double) deck2[k] / deck2[0];
                            Deck deck3 = draw(deck2, k);
                            int d = indexer[deck3];
                            double tmp = compute_ev(d, i, j, k);
                            //double tmp2 = compute_ev(d, j, i, k);
                            //printf("%d %d %d %.10f %.10f %.10f \n", i,j,k,p1*p2*p3, tmp, tmp2); 
                            ev += p1*p2*p3*tmp;
                        }
                    }
                }
            }
        }
    }
    return ev;
}


int recurse_decks(Deck d, int total) { 
    if(indexer[d] != 0 and cache[d] <= total)
        return indexer[d];
    else if(indexer[d] == 0) {
        indexer[d] = deck_number++;
        for(int i=0; i<11; i++)
            decks[indexer[d]][i] = d[i];
    }
    cache[d] = total;
    for(int c=1; c < min(22-total, 11); c++) {
        int idx = recurse_decks(draw(d, c), total+c);
        deck_mapping[indexer[d]][c] = idx;
    }
    return indexer[d]; 
}

int all_subdecks(Deck d) {
    for(int i=1; i < 11; i++)
        for(int j=1; j < 11; j++)
            for(int k=1; k < 11; k++) { 
                Deck d2 = draw(draw(draw(d, i), j), k);
                recurse_decks(d2, j+k);
                if(j == k)
                    recurse_decks(d2, j);
            }
    return deck_number;
}

void best_first_action(int deck, int card1, int card2, int dealer) {
    bool soft = (card1 == 1) || (card2 == 1);
    int total = card1 + card2 + 10*soft;
    double S = ev_stand(deck, total, dealer);
    double H = -2, D = -2, P = -2, R = -0.5;
    R = -2.0; // No Surrender
    if(soft) {
        H = ev_hit_soft(deck, total, dealer);
        D = ev_double_soft(deck, total, dealer);
    } else {
        H = ev_hit_hard(deck, total, dealer);
        D = ev_double_hard(deck, total, dealer);
    }
    if(card1 == card2)
        P = ev_split(deck, card1, dealer); 
    printf("%lf %lf %lf %lf %lf\n", H, S, D, P, R);
}

void best_rest_action(int deck, int total, int dealer, bool soft) {
    double S = ev_stand(deck, total, dealer);
    double H;
    if(soft) 
        H = ev_hit_soft(deck, total, dealer);
    else
        H = ev_hit_hard(deck, total, dealer);
    printf("%lf %lf\n", H, S);
}

int main(int argc, char **argv) {
    /* This program has three modes, depending on the number of input arguments
    
    Arguments 1-10 correspond to the deck composition, and specify the number of A's, 2's, ..., 10's

        With no more arguments, we simply return the EV of the game.

    Argument 11 [Optional] corresponds to the dealer upcard
    Arguments 12 and 13 correspond to our hole cards

        With no more argumetns, we return the EV of five actions {Hit,Stand,Double,Split,Surrender}

    Arguments 14+ correspond to additional cards we have taken
        
        we return the EV of two actions {Hit,Stand}

    */
    Deck deck;
    if(argc >= 11) {
        deck = {0,0,0,0,0,0,0,0,0,0,0};
        for(int i=1; i < 11; i++) {
            deck[i] = stoi(argv[i]);
            deck[0] += deck[i];
        }
    } else {
        deck = {52,4,4,4,4,4,4,4,4,4,16};
    }

    initialize();   
    dealer_sequences();

    if(argc <= 11) {
        all_subdecks(deck);
        double ev = expected_value_of_game(deck);
        cout << ev << endl;
    } else {
        Deck deck2 = deck;
        for(int i=11; i < argc; i++) {
            int card = stoi(argv[i]);
            deck2 = add(deck2, card);
        }
        all_subdecks(deck2);
        int dealer = stoi(argv[11]);
        int card1 = stoi(argv[12]);
        int card2 = stoi(argv[13]);
        int d = indexer[deck];
        if(argc == 14)
            best_first_action(d, card1, card2, dealer);
        else {
            int total = card1 + card2;
            bool ace = (card1 == 1) || (card2 == 1);
            for(int i=14; i < argc; i++) {
                total += stoi(argv[i]);
                ace = ace || (stoi(argv[i]) == 1);
            }
            if(ace && total+10 <= 21)
                best_rest_action(d, total+10, dealer, true);
            else 
                best_rest_action(d, total, dealer, false);
        }
    }
}
