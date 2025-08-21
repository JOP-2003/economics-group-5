import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("economy.csv")  # put your CSV next to this script

# One plot per metric (keep plots separate)
plt.figure(); plt.plot(df["Step"], df["Capital"]);   plt.title("Capital over Steps");   plt.xlabel("Step"); plt.ylabel("Capital");   plt.tight_layout(); plt.show()
plt.figure(); plt.plot(df["Step"], df["Wealth"]);    plt.title("Wealth over Steps");    plt.xlabel("Step"); plt.ylabel("Wealth");    plt.tight_layout(); plt.show()
plt.figure(); plt.plot(df["Step"], df["Profit"]);    plt.title("Profit over Steps");    plt.xlabel("Step"); plt.ylabel("Profit");    plt.tight_layout(); plt.show()
plt.figure(); plt.plot(df["Step"], df["Inflation"]); plt.title("Inflation over Steps"); plt.xlabel("Step"); plt.ylabel("Inflation"); plt.tight_layout(); plt.show()
