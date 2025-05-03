import yfinance as yf
import pandas as pd

symbols = ["META", "GOOGL"]
interval = "1m"
period = "5d"

df_all = {}
for symbol in symbols:
    ticker = yf.Ticker(symbol)
    hist = ticker.history(period=period, interval=interval)
    df_all[symbol] = hist["Close"]

df_combined = pd.concat(df_all, axis=1)
df_combined.dropna(inplace=True)

df_combined.to_csv("intraday_prices.csv", index=True) 
print("Saved to intraday_prices.csv")