#include <cmath>
#include <iostream>
#include <fstream>
#include <iomanip>
using namespace std;

class Economy
{
public:
    // --------- Business variables ----------
    float buis_profit = 0.0f;     // profit
    float buis_capital = 1000.0f; // capital
    float loan = 200.0f;

    // between business and households
    float mpc = 0.4f;
    float mps = 0.2f;

    // --------- Household variables ----------
    float house_income = 0.0f;      // initialize to avoid UB
    float house_wealth = 1000.0f;
    float savings = 100.0f;
    float last_house_income = 0.0f; // track previous period

    // --------- Banking / policy aggregates ----------
    float reserve_ratio = 20.0f;
    float bank_wealth = 1000.0f;

    float tax_revenue = 1000.0f;
    float interest_rate = 3.0f;   // PERCENT (e.g., 3 = 3%)

    float inflation = 2.0f;       // PERCENT (e.g., 2 = 2%)
    float expenses = 100.0f;
    float expenses_Inf = 0.0f;

    float tax_rate = 20.0f;       // %
    float tax_target = 20.0f;     // anchor for mild mean-reversion
    float gov_Spnd = 0.4f;
    float wages = 0.0f;
    float wagMult = 0.15f;

    // ---------- diagnostics --------------
    int check_inflation()
    {
        if (inflation < 1.0f) return -1;                        // low
        if (inflation >= 1.0f && inflation <= 3.0f) return 0;   // moderate
        if (inflation > 3.0f) return 1;                         // high
        return 0; // fallback
    }

    int check_interest()
    {
        if (interest_rate < 2.0f) return -1;                          // low
        if (interest_rate >= 2.0f && interest_rate <= 5.0f) return 0; // moderate
        if (interest_rate > 5.0f) return 1;                           // high
        return 0; // fallback
    }

    // ---------- government ----------
    void set_Gov_Spend()
    {
        // Base: spend most of what you collect (very simple)
        float base = 0.8f * tax_revenue;

        // Counter-cyclical adjustments
        float adj = 0.0f;

        // If income is falling or firms are losing money, add stimulus
        if (house_income < last_house_income || buis_profit < 0.0f)
            adj += 0.2f * tax_revenue;

        // If inflation is high, trim spending; if very low, add a bit
        int infl = check_inflation(); // -1 low, 0 moderate, 1 high
        if (infl == 1)       adj -= 0.2f * tax_revenue;
        else if (infl == -1) adj += 0.1f * tax_revenue;

        // If interest is high, be a bit more conservative
        if (check_interest() == 1)
            adj -= 0.1f * tax_revenue;

        gov_Spnd = base + adj;
        if (gov_Spnd < 0.0f) gov_Spnd = 0.0f; // no negative spend
    }

    void changeTaxRate()
    {
        float delta = 0.0f;

        // Inflation signal (very mild)
        int infl = check_inflation(); // -1 low, 0 moderate, 1 high
        if (infl == 1)       delta += 0.10f;  // cool demand a bit
        else if (infl == -1) delta -= 0.10f;  // support demand a bit

        // Activity / stress: if weak, ease taxes
        if (house_income < last_house_income || buis_profit < 0.0f)
            delta -= 0.05f;

        // If rates already high, lean looser fiscally
        if (interest_rate > 5.0f)
            delta -= 0.05f;

        // Gentle mean reversion toward anchor
        delta += (tax_target - tax_rate) * 0.01f;

        // Apply + clamp
        tax_rate += delta;
        if (tax_rate < 0.0f)  tax_rate = 0.0f;
        if (tax_rate > 60.0f) tax_rate = 60.0f;
    }

    void Gov_step()
    {
        set_Gov_Spend();
        changeTaxRate();
    }

    // ---------- firm / household dynamics ----------
    void Buis_Step()
    {
        float preProf = buis_profit;

        // profit = sales proxy - taxes - costs - interest/repayment
        // sales proxy = household demand + expenses + gov spend
        if (loan > 0.0f)
        {
            buis_profit = (house_income * mpc + expenses + gov_Spnd);

            // taxes and other "expenses" proxy (simple)
            buis_profit -= (buis_profit * tax_rate / 100.0f) + (0.35f * preProf);

            // apply interest as percentage on loan + simple principal amortization
            float loan_payment = loan * (interest_rate / 100.0f) + (loan / 24.0f);
            buis_profit -= loan_payment;
            loan -= loan_payment;
            if (loan < 0.0f) loan = 0.0f;

            tax_revenue += (preProf * tax_rate / 100.0f);
        }
        else
        {
            // take a new loan if none outstanding
            loan = 0.7f * buis_capital;
            buis_capital += loan;

            buis_profit = (house_income * mpc + expenses + gov_Spnd);
            buis_profit -= (buis_profit * tax_rate / 100.0f) + (0.35f * preProf);

            float loan_payment = loan * (interest_rate / 100.0f) + (loan / 24.0f);
            buis_profit -= loan_payment;
            loan -= loan_payment;
            if (loan < 0.0f) loan = 0.0f;

            tax_revenue += (preProf * tax_rate / 100.0f);
        }

        buis_capital += buis_profit;
        wages = wagMult * buis_profit;
        buis_capital -= wages;

        House_step();
    }

    void House_step()
    {
        // income = wages + interest on savings - expenses
        float preInc = house_income;

        // apply interest on savings as percentage
        house_income = wages + (savings * interest_rate / 100.0f) - expenses;

        // simple adjustment with MPC/MPS to changes in income
        house_income = house_income - ((preInc - house_income) * mpc)
                                    - ((preInc - house_income) * mps);

        house_wealth += house_income;
        savings += (house_income * mps);

        changeInflation();
        changeInterest();
        changeMPC();
        changeMPS();
        changeExps();

        // track income trend for next step
        last_house_income = house_income;

        // update government after household updates
        Gov_step();
    }

    // ---------- simple macro rules ----------
    void changeInflation()
    {
        float delta = 0.0f;

        // demand vs cost proxies (heuristic)
        float demand_proxy = house_income * mpc + gov_Spnd; // private + public demand
        float cost_proxy   = expenses;                       // crude cost stand-in

        if (demand_proxy > cost_proxy)      delta += 0.10f;
        else if (demand_proxy < cost_proxy) delta -= 0.10f;

        // profitability pressure
        if (buis_profit > 0.0f)      delta += 0.05f;
        else if (buis_profit < 0.0f) delta -= 0.05f;

        // monetary policy
        if (interest_rate > 4.0f)      delta -= 0.10f;
        else if (interest_rate < 2.0f) delta += 0.05f;

        // mean reversion toward 2%
        delta += (2.0f - inflation) * 0.02f;

        // apply & clamp
        inflation += delta;
        if (inflation < 0.0f)  inflation = 0.0f;
        if (inflation > 20.0f) inflation = 20.0f;
    }

    void changeInterest()
    {
        // Keeping your existing policy logic (adjusts the rate level).
        // Note: this is not applied as a percentage—it's the rate itself.
        if (inflation > 4.0f) { interest_rate += 1.0f; }
        if (inflation < 1.0f) { interest_rate -= 1.0f; }
    }

    int changeMPC()
    {
        int c = check_inflation();
        if (c == 1)       mpc = mpc + 0.01f;
        else if (c == -1) mpc = mpc - 0.01f;
        // clamp to [0,1] for sanity
        if (mpc < 0.0f) mpc = 0.0f;
        if (mpc > 1.0f) mpc = 1.0f;
        return 1;
    }

    int changeMPS()
    {
        // adjust mps based on interest level
        int c = check_interest();
        if (c == 1)       mps = mps + 0.01f;
        else if (c == -1) mps = mps - 0.01f;
        // clamp to [0,1] and keep mpc+mps <= 1 simple guard
        if (mps < 0.0f) mps = 0.0f;
        if (mps > 1.0f) mps = 1.0f;
        if (mpc + mps > 1.0f) mps = 1.0f - mpc;
        return 1;
    }

    void changeExps()
    {
        // apply inflation as a percentage change to expenses
        expenses *= (1.0f + inflation / 100.0f);
    }
};

int main()
{
    Economy econ; // create economy object

    ofstream file("economy.csv");
    if (!file.is_open()) {
        cerr << "Could not open economy.csv for writing.\n";
        return 1;
    }

    // CSV header
    file << "Step,"
         << "Profit,"
         << "Capital,"
         << "Income,"
         << "Wealth,"
         << "Savings,"
         << "Inflation,"
         << "Interest,"
         << "TaxRate,"
         << "GovSpend,"
         << "Loan,"
         << "Wages,"
         << "MPC,"
         << "MPS"
         << "\n";

    // Run 100 cycles
    for (int i = 0; i < 100; i++) {
        econ.Buis_Step();

        file << i << ","
             << econ.buis_profit << ","
             << econ.buis_capital << ","
             << econ.house_income << ","
             << econ.house_wealth << ","
             << econ.savings << ","
             << econ.inflation << ","
             << econ.interest_rate << ","
             << econ.tax_rate << ","
             << econ.gov_Spnd << ","
             << econ.loan << ","
             << econ.wages << ","
             << econ.mpc << ","
             << econ.mps
             << "\n";
    }

    file.close();
    cout << "Simulation complete. Wrote 100 steps to economy.csv\n";
    return 0;
}
