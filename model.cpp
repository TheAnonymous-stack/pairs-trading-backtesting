#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <cmath>

using namespace std;
#define WINDOW_SIZE 60
#define INITIAL_CAPITAL 100000.0

void readPrices(const string& filename, vector<double>& meta_prices, vector<double>& googl_prices) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error opening file\n";
        return;
    }

    string line;
    getline(file, line); // skip header
    int i = 0;

    while (getline(file, line)) {
        stringstream ss(line);
        string timestamp, meta_str, googl_str;
        getline(ss, timestamp, ',');
        getline(ss, meta_str, ',');
        getline(ss, googl_str, ',');
        double meta_price = stod(meta_str);
        double googl_price = stod(googl_str);
        meta_prices[i] = meta_price;
        googl_prices[i] = googl_price;
        ++i;
    }
    file.close();
}

void calculateLogReturns(vector<double>& prices, vector<double>& log_returns) {
    for (size_t i = 1; i < prices.size(); i++) {
        log_returns[i - 1] = log(prices[i]/prices[i - 1]);
    }
}

double calculateHedgeRatio(const vector<double>& meta_returns, const vector<double>& googl_returns) {
    // Use simple linear regression to find hedge ratio
    // The hedge ratio is derived from an unbiased estimator that minimizes the sum of squares of residuals
    double meta_mean = 0.0;
    double googl_mean = 0.0;
    double Sxx = 0.0;
    for (int i = 0; i < meta_returns.size(); ++i) {
        meta_mean += meta_returns[i];
        googl_mean += googl_returns[i];
        Sxx += (googl_returns[i] * googl_returns[i]);
    }
    meta_mean /= meta_returns.size();
    googl_mean /= googl_returns.size();
    Sxx -= (googl_returns.size() * googl_mean * googl_mean);
    double Sxy = 0.0; 
    for (int i = 0; i < meta_returns.size(); ++i) {
        Sxy += (googl_returns[i] - googl_mean) * (meta_returns[i] - meta_mean);
    }
    return Sxy/Sxx;
}

vector<double> computeSpread(vector<double>& meta_returns, vector<double>& googl_returns, double hedge_ratio) {
    vector<double> spreads(1949);
    for (int i = 0; i < meta_returns.size(); ++i) {
        spreads[i] = meta_returns[i] - hedge_ratio * googl_returns[i];
    }
    return spreads;
}


void getRollingMeansAndStd(vector<double>& spreads, vector<double>& rollingMeans, vector<double>& rollingStd) {
    for (int i = WINDOW_SIZE - 1; i < spreads.size(); ++i) {
        double sum = 0.0;
        for (int j = i - WINDOW_SIZE + 1; j < i + 1; ++j) {
            sum += spreads[j];
        }
        double mean = sum / WINDOW_SIZE;
        rollingMeans[i - (WINDOW_SIZE - 1)] = mean;

        double sum_residual_sq = 0.0;
        for (int j = i - WINDOW_SIZE + 1; j < i + 1; ++j) {
            sum_residual_sq += ((spreads[j] - mean) * (spreads[j] - mean));
        }
        rollingStd[i - (WINDOW_SIZE - 1)] = sqrt(sum_residual_sq / (WINDOW_SIZE - 1));
    }
}

void getMoment(int index) {
    // getMoment prints the day and time in intraday_prices.csv associated to the specified index
    int day = static_cast<int> (index / 390) + 1;
    int minute_in_the_day = index % 390;
    int hour = static_cast<int> (minute_in_the_day / 60);
    int extra_minute = minute_in_the_day - hour * 60;
    int moment_hour = 9 + hour;
    int moment_minute = 30 + extra_minute;
    if (moment_minute > 59) {
        ++moment_hour;
        moment_minute -= 60;
    }

    cout << "Day is " << day << endl;
    cout << "Hour is " << moment_hour << endl;
    cout << "Minute is "<< moment_minute << endl;

}


int main() {
    vector<double> meta_prices(1950), googl_prices(1950);
    readPrices("intraday_prices.csv",meta_prices, googl_prices);

    vector<double> meta_returns(1949), googl_returns(1949);

    // paralellize calculation of log returns for each asset
    thread t1(calculateLogReturns, ref(meta_prices), ref(meta_returns));
    thread t2(calculateLogReturns, ref(googl_prices), ref(googl_returns));

    t1.join();
    t2.join();


    double hedgeRatio = calculateHedgeRatio(meta_returns, googl_returns);

    vector<double> spreads = computeSpread(meta_returns, googl_returns, hedgeRatio);
    
    // 1 minute interval over 5 days means 1950 data points. 
    vector<double> rollingMeans(1950 - (WINDOW_SIZE - 1)); 
    vector<double> rollingStd(1950 - (WINDOW_SIZE - 1));
    getRollingMeansAndStd(spreads, rollingMeans, rollingStd);

    double max_googl_allocation, max_meta_allocation;
    double capital = INITIAL_CAPITAL;
    int googl_shares_long, googl_shares_short, meta_shares_long, meta_shares_short;
    googl_shares_long = googl_shares_short = meta_shares_long = meta_shares_short = 0;
    int has_position_above = 0;
    int has_position_below = 0;

    int i;
    for (i = 0; i < 1950 - (WINDOW_SIZE - 1); ++i) {
        double z_score = (spreads[i + WINDOW_SIZE - 1] - rollingMeans[i]) / rollingStd[i];
        if (z_score > 1 && capital > googl_prices[i + WINDOW_SIZE - 1]) { 
            cout << "short meta long googl..." << endl;
            has_position_above = 1;

            getMoment(i + WINDOW_SIZE - 1);
            cout << "current capital is " << capital << endl;
            cout << "current meta shares to short is " << meta_shares_short << endl;
            cout << "current meta shares to long is " << meta_shares_long << endl;
            cout << "current googl_shares to long is " << googl_shares_long << endl;
            cout << "current googl_shares to short is " << googl_shares_short << endl;
            
            cout << "meta price is " << meta_prices[i + WINDOW_SIZE - 1] << endl;
            cout << "googl price is " << googl_prices[i + WINDOW_SIZE - 1] << endl;

            max_googl_allocation = capital / (1 + hedgeRatio);
            cout << "max_googl_allocation is " << max_googl_allocation << endl;

            max_meta_allocation = hedgeRatio * max_googl_allocation;
            cout << "max_meta_allocation is " << max_meta_allocation << endl;

            int additional_googl_shares_long = static_cast<int> (max_googl_allocation / googl_prices[i + WINDOW_SIZE - 1]);
            cout << "additional_googl_shares_long is " << additional_googl_shares_long << endl;

            googl_shares_long += additional_googl_shares_long;
            
            int additional_meta_shares_short = static_cast<int> (max_meta_allocation / meta_prices[i + WINDOW_SIZE - 1]);
            cout << "additional_meta_shares_short is " << additional_meta_shares_short << endl;

            meta_shares_short += additional_meta_shares_short;
            capital -= (additional_googl_shares_long * googl_prices[i + WINDOW_SIZE - 1]);
            capital += (additional_meta_shares_short * meta_prices[i + WINDOW_SIZE - 1]); // treat short proceeds as part of capital
            cout << "capital after processing order is " << capital << endl;
        } 
        else if (z_score < -1 && capital > meta_prices[i + WINDOW_SIZE - 1]) {
            cout << "short googl long meta..." << endl;
            has_position_below = 1;

            getMoment(i + WINDOW_SIZE - 1);
            cout << "current capital is " << capital << endl;
            cout << "current meta shares to short is " << meta_shares_short << endl;
            cout << "current meta shares to long is " << meta_shares_long << endl;
            cout << "current googl_shares to long is " << googl_shares_long << endl;
            cout << "current googl_shares to short is " << googl_shares_short << endl;

            cout << "meta price is " << meta_prices[i + WINDOW_SIZE - 1] << endl;
            cout << "google price is " << googl_prices[i + WINDOW_SIZE - 1] << endl;

            max_googl_allocation = capital / (1 + hedgeRatio);
            cout << "max_googl_allocation is " << max_googl_allocation << endl;

            max_meta_allocation = hedgeRatio * max_googl_allocation;
            cout << "max_meta_allocaiton is " << max_meta_allocation << endl;

            int additional_googl_shares_short = static_cast<int> (max_googl_allocation / googl_prices[i + WINDOW_SIZE - 1]);
            cout << "additional_googl_shares_short is " << additional_googl_shares_short << endl;

            googl_shares_short += additional_googl_shares_short;

            int additional_meta_shares_long = static_cast<int> (max_meta_allocation / meta_prices[i + WINDOW_SIZE - 1]);
            cout << "additional_meta_shares_long is " << additional_meta_shares_long << endl;

            meta_shares_long += additional_meta_shares_long;

            capital -= (additional_meta_shares_long * meta_prices[i + WINDOW_SIZE - 1]);
            capital += (additional_googl_shares_short * googl_prices[i + WINDOW_SIZE - 1]);
            cout << "capital after processing order is " << capital << endl;

        } 
        else if (0 < z_score && z_score < 1 && has_position_above) {
            cout << "found exit condition for has_position_above" << endl;
            getMoment(i + 59);
            cout << "google price is " << googl_prices[i + WINDOW_SIZE - 1] << endl;
            cout << "google share for long is " << googl_shares_long << endl;
            cout << "google share for short is " << googl_shares_short << endl;

            cout << "meta price is " << meta_prices[i + WINDOW_SIZE - 1] << endl;
            cout << "meta share for long is " << meta_shares_long << endl;
            cout << "meta share for short is " << meta_shares_short << endl;

            
            has_position_above = 0;
            // liquidate relevant assets
            capital += (googl_shares_long * googl_prices[i + WINDOW_SIZE - 1] - meta_shares_short * meta_prices[i + WINDOW_SIZE - 1]);
            cout << "capital after liquidating is " << capital << endl;
            googl_shares_long = meta_shares_short = 0;
        } 
        else if (-1 < z_score && z_score < 0 && has_position_below) {
            cout << "found exit condition for has_position_below" << endl;
            getMoment(i + 59);
            cout << "google price is " << googl_prices[i + WINDOW_SIZE - 1] << endl;
            cout << "google share for long is " << googl_shares_long << endl;
            cout << "google share for short is " << googl_shares_short << endl;

            cout << "meta price is " << meta_prices[i + WINDOW_SIZE - 1] << endl;
            cout << "meta share for long is " << meta_shares_long << endl;
            cout << "meta share for short is " << meta_shares_short << endl;

            
            has_position_below = 0;
            // liquidate relevant assets
            capital += (meta_shares_long * meta_prices[i + WINDOW_SIZE - 1] - googl_shares_short * googl_prices[i + WINDOW_SIZE - 1]);
            cout << "capital after liquidating is " << capital << endl;
            meta_shares_long = googl_shares_short = 0;
        }
    }
    if (has_position_above || has_position_below) {
        // force close at last timestamp
        cout << "force closed" << endl;
        capital += (googl_shares_long - googl_shares_short) * googl_prices[i - 1 + WINDOW_SIZE - 1] + (meta_shares_long - meta_shares_short) * meta_prices[i - 1 + WINDOW_SIZE - 1];
        googl_shares_long = googl_shares_short = meta_shares_long = meta_shares_short = 0;
        has_position_above = has_position_below = 0;
    }

    double pnl = capital - INITIAL_CAPITAL;
    cout << "Final Capital is " << capital << endl;
    cout << "PnL is " << pnl << endl;
    return 0;  
}