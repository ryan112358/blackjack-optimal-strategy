import numpy as np
from subprocess import Popen,PIPE
from multiprocessing import Pool
import pandas as pd

def hand_ev(deck):
    counts = [str(x) for x in np.bincount(deck, minlength=11)[1:]]
    p = Popen(['./a.out'] + counts, stdout=PIPE)
    return float(p.communicate()[0])

def hand_moves(deck, cards_in_play, hole):
    counts = list(np.bincount(deck, minlength=11)[1:])
    counts[hole-1] += 1
    inputs = [str(x) for x in counts + cards_in_play]
    p = Popen(['./a.out'] + inputs, stdout=PIPE)
    return [float(x) for x in p.communicate()[0].split(b' ')]

def is_blackjack(A,B):
    return (A==1 and B==10) or (A==10 and B==1)

def dealer_done(total, ace):
    if total >= 17:
        return True
    elif ace and total >= 8 and total <= 11:
        return True
    return False

def dealer_draw(dealer, hole, deck):
    total = dealer + hole
    ace = (dealer == 1) or (hole == 1)
    while not dealer_done(total, ace):
        card = deck.pop()
        total += card
        ace = ace or (card == 1)
    if ace and total <= 11:
        total += 10
    return total

def player_draw(card1, card2, dealer, deck, hole):
    cards_in_play = [dealer, card1, card2]
    total = card1 + card2
    ace = (card1==1) or (card2==1)
    while total <= 21:
        evs = hand_moves(deck, cards_in_play, hole)[:2]
        H, S = evs
        if S > H:
            if ace and total <= 11:
                print('Standing Soft %d vs. Dealer %d' % (total+10, dealer))
            else:
                print('Standing Hard %d vs. Dealer %d' % (total, dealer))
            #print('Standing', H, S)
            break
        else:
            if ace and total <= 11:
                print('Hitting Soft %d vs. Dealer %d' % (total+10, dealer))
            else:
                print('Hitting Hard %d vs. Dealer %d' % (total, dealer))

            card = deck.pop()
            cards_in_play.append(card)
            total += card
            ace = ace or (card==1)
        #print('Player Cards', cards_in_play[1:], total)
    if ace and total <= 11:
        total += 10
    return total

def player_double(card1, card2, dealer, deck):
    card3 = deck.pop()
    total = card1 + card2 + card3
    if(total + 10 <= 21 and (card1 == 1 or card2 == 1 or card3 == 1)):
        total += 10
    return total

def win_lose_draw(pt, dt):
    if pt > 21:
        return -1
    elif dt > 21 or pt > dt:
        return 1
    elif pt < dt:
        return -1
    return 0
    
def play_hand(deck):
    ev = hand_ev(deck)
    card1 = deck.pop()
    dealer = deck.pop()
    card2 = deck.pop()
    hole = deck.pop()
    
    if is_blackjack(card1, card2) and is_blackjack(dealer, hole):
        val = 0
        return ev, val, 'BJ', val
    if is_blackjack(card1, card2) and not is_blackjack(dealer, hole):
        val = 1.5
        return ev, val, 'BJ', val
    if not is_blackjack(card1, card2) and is_blackjack(dealer, hole):
        val = -1
        return ev, val, 'BJ', val

    cards_in_play = [dealer, card1, card2]
    evs = hand_moves(deck, cards_in_play, hole)
    moves = ['H','S','D','P','R']
    move = moves[np.argmax(evs)]
    #print('Move', cards_in_play, move)
    if move == 'R':
        if card1 == 1 or card2 == 1:
            print('Surrendering Soft %d vs. Dealer %d' % (card1+card2+10, dealer))
        else:
            print('Surrendering Hard %d vs. Dealer %d' % (card1+card2, dealer))
        val = -0.5
    elif move == 'D':
        if card1 == 1 or card2 == 1:
            print('Doubling Soft %d vs. Dealer %d' % (card1+card2+10, dealer))
        else:
            print('Doubling Hard %d vs. Dealer %d' % (card1+card2, dealer))
        total = player_double(card1, card2, dealer, deck)
        dt = dealer_draw(dealer, hole, deck)
        val = 2*win_lose_draw(total, dt)
    elif move == 'P':
        print('Splitting %d\'s vs. Dealer %d' % (card1, dealer))
        assert card1 == card2
        card1b = deck.pop()
        card2b = deck.pop()
        if card1 == 1:
            total_A = card1 + card1b + 10
            total_B = card2 + card2b + 10
            bet_A = bet_B = 1
        else:
            cards_in_play = [dealer, card1, card1b]
            evs = hand_moves(deck, cards_in_play, hole)
            move = moves[np.argmax(evs[:3])]
            if move == 'D':
                print('Double after splitting!')
                total_A = player_double(card1, card1b, dealer, deck)
                bet_A = 2
            else:
                total_A = player_draw(card1, card1b, dealer, deck, hole)
                bet_A = 1
            cards_in_play = [dealer, card2, card2b]
            evs = hand_moves(deck, cards_in_play, hole)
            move = moves[np.argmax(evs[:3])]
            if move == 'D':
                print('Double after splitting!')
                total_B = player_double(card2, card2b, dealer, deck)
                bet_B = 2
            else:
                total_B = player_draw(card2, card2b, dealer, deck, hole)
                bet_B = 1
        dt = dealer_draw(dealer, hole, deck)
        val = win_lose_draw(total_A, dt)*bet_A + win_lose_draw(total_B, dt)*bet_B
    else:
        total = player_draw(card1, card2, dealer, deck, hole)
        dt = dealer_draw(dealer, hole, deck)
        val = win_lose_draw(total, dt)
    return ev, val, move, np.max(evs)


def shoe_game(seed):
    prng = np.random.RandomState(seed)
    
    deck = 8*np.array([4,4,4,4,4,4,4,4,4,16]);
    deck = list(prng.permutation(np.repeat(np.arange(1,11), deck)))
    
    evs = []
    vals = []
    counts = []
    pen = []
    while len(deck) > 52*1:
        pen.append(len(deck) / 8 / 52)
        binct = np.bincount(deck)
        count = binct[1] + binct[10] - binct[2:7].sum()
        ev, val, _, _ = play_hand(deck)
        evs.append(ev)
        vals.append(val)
        counts.append(count)
        print(ev, val)

    df = pd.DataFrame()
    df['EV'] = evs
    df['AV'] = vals
    df['count'] = counts
    df['penetration'] = pen

    with open('results.csv', 'a') as f:
        df.to_csv(f, index=False, header=f.tell()==0)

    return df

def one_hand_fixed_comp(seed):
    prng = np.random.RandomState(seed)
    deck = np.array([4,4,4,4,4,4,4,4,4,16]);
    deck = list(prng.permutation(np.repeat(np.arange(1,11), deck)))
    
    ev, val, move, evmove = play_hand(deck)
    df = pd.DataFrame()
    df['EV'] = [ev]
    df['AV'] = [val]
    df['move'] = [move]
    df['EVmove'] = [evmove]

    with open('debug-onedeck.csv', 'a') as f:
        df.to_csv(f, index=False, header=f.tell()==0)

    return df
    

# TODO(ryan): add function to run simulations with fixed deck composition
# Verify that AV matches EV 
# If not, see specifically where it disagrees (card1, card2, dealer)


while True:
    p = Pool(4)
    seeds = np.random.randint(0, 2**30, 10000000)
    results = p.map(one_hand_fixed_comp, seeds)
    df = pd.concat(results)

"""
p = Pool(10)
seeds = np.random.randint(0, 2**30, 100000)
results = p.map(shoe_game, seeds)
"""
#df = pd.concat(results)
#with open('results.csv', 'a') as f:
#    df.to_csv(f, index=False, header=f.tell()==0)


