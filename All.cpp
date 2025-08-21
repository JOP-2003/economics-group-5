#include <cmath>
#include <iostream>


using namespace std;

int main() 
{
    
}

class Economy
{
    public:

    //Buisness variables
    float buis_profit = 0; // profit = sales -taxes - wages - (gov help multiplier)costs of production - interest on loan (eventuall - loan pay)
    // sales = G and S sold to households and gov
    float buis_capital = 1000; // capital = capital + profit + loans 
    float loan = 200;

    //between buis and households
    float mpc = 0.4;
    float mps = 0.2;



    // Household variables
    float house_income; // income = wages + interest on savings - expenses (income * MPC) - savings(income * (1-mpc))
    float house_wealth = 1000; // wealth = wealth + income + savings
    float savings = 100;

    float last_house_income = 0.0f; // track previous period household income



    float reserve_ratio = 20;
    float bank_wealth = 1000;

    float tax_revenue = 1000;
    float interest_rate = 3;


    float inflation = 2;
    float expenses = 100;
    float expenses_Inf = 0;

    float tax_rate = 20;
    float gov_Spnd = 0.4;
    float wages = 0;
    float wagMult = 0.15;


    



    int check_inflation()
    {
        if(inflation < 1)
        {
            return -1; // low inflation
        }
        if(inflation > 1 && inflation < 3)
        {
            return 0; // moderate inflation
        }
        if(inflation > 3)
        {
            return 1; // high inflation
        }
    }

    int check_interest()
    {
        if(interest_rate < 2)
        {
            return -1; // low interest
        }
        if(interest_rate > 2 && interest_rate < 5)
        {
            return 0; // moderate interest
        }
        if(interest_rate > 5)
        {
            return 1; // high interest
        }
    }




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


    void Buis_Step()
    {
        
        float preProf = buis_profit;
        //profit = sales -taxes - wages(move to capital) - (gov help multiplier)costs of production - interest on loan (eventuall - loan pay)
        if(loan > 0)
        {
            
            buis_profit = (house_income*mpc + expenses+ gov_Spnd) ;
            buis_profit = buis_profit - (buis_profit*tax_rate/100) - (0.35*preProf) - (loan*interest_rate + (loan/24));
                                        //taxes                      // expenses
            loan = loan - (loan*interest_rate+(loan/24));
            tax_revenue = tax_revenue + (preProf*tax_rate/100);
        }

        if(loan <= 0.0f)
        {
            loan = 0.7*buis_capital;
            buis_capital = buis_capital + loan;
            buis_profit = (house_income*mpc + expenses+ gov_Spnd) ;
            buis_profit = buis_profit - (buis_profit*tax_rate/100) - (0.35*preProf) - (loan*interest_rate + (loan/24));
                                        //taxes                      // expenses
            loan = loan - (loan*interest_rate+(loan/24));
            tax_revenue = tax_revenue + (preProf*tax_rate/100);
        }
        
        buis_capital = buis_capital + buis_profit;
        wages = wagMult*buis_profit;
        buis_capital = buis_capital - wages;
        House_step();

        

    }

    void House_step()
    {
        // income = wages + interest on savings - expenses (income * MPC) - savings(income * (1-mpc))
        
        float preInc = house_income;
        house_income = wages + (interest_rate*savings) - expenses ;
        house_income = house_income -((preInc -house_income)*mpc) - ((preInc -house_income)*mps);
        house_wealth = house_wealth + house_income;
        savings = savings + (house_income*mps);

        changeInflation();
        changeInterest();
        changeMPC();
        changeMPS();
        changeExps();

        // track income trend for next step
        last_house_income = house_income;

        // update government spending for the next business step
        
        Gov_step();

    }

    void Gov_step()
    {
        set_Gov_Spend();
        changeTaxRate();

    }





    void changeInflation()
    {
        // Heuristics:
        // - If effective demand > costs, prices face upward pressure.
        // - Profits > 0 add a little upward pressure; losses reduce it.
        // - Higher interest rates cool inflation; very low rates warm it.
        // - Small mean reversion toward a 2% target.
        // Keep everything very lightweight and linear.

        float delta = 0.0f;

        // Very simple "demand" proxy vs. "costs"
        float demand_proxy = house_income * mpc + gov_Spnd; // private + public demand
        float cost_proxy   = expenses;                       // production/household costs stand‑in

        if (demand_proxy > cost_proxy)      delta += 0.10f;   // demand-pull up
        else if (demand_proxy < cost_proxy) delta -= 0.10f;   // weak demand, down

        // Profitability pressure (cost‑push/markups)
        if (buis_profit > 0.0f)      delta += 0.05f;
        else if (buis_profit < 0.0f) delta -= 0.05f;

        // Monetary policy pressure
        if (interest_rate > 5.0f)      delta -= 0.10f;  // tighter policy cools
        else if (interest_rate < 2.0f) delta += 0.05f;  // easy policy warms

        // Gentle mean reversion toward 2%
        delta += (2.0f - inflation) * 0.02f;

        // Apply and clamp to a simple range
        inflation += delta;
        if (inflation < 0.0f)  inflation = 0.0f;
        if (inflation > 20.0f) inflation = 20.0f;
    }



    void changeInterest()
    {
        if(inflation > 4)
        {
            interest_rate++;
        }
        if(inflation < 1)
        {
            interest_rate++;
        }
    }

    int changeMPC()
    {
        if(check_inflation() == 0)
        {
            return 1;
        }
        if(check_inflation() == 1)
        {
            mpc = mpc + 0.01;
            return 1;
        }
        if(check_inflation() == -1)
        {
            mpc = mpc - 0.01;
            return 1;
        }
    }

    int changeMPS()
    {
        if(check_interest() == 0)
        {
            return 1;
        }
        if(check_interest() == 1)
        {
            mpc = mpc + 0.01;
            return 1;
        }
        if(check_interest() == -1)
        {
            mpc = mpc - 0.01;
            return 1;
        }
    }

    void changeExps()
    {
        expenses = expenses + expenses*inflation;
    }


    void changeTaxRate()
{
    float delta = 0.0f;

    // Inflation signal (very mild)
    int infl = check_inflation(); // -1 low, 0 moderate, 1 high
    if (infl == 1)       delta += 0.10f;  // cool demand a bit
    else if (infl == -1) delta -= 0.10f;  // support demand a bit

    // Activity / stress signal: weak income trend or losses → ease taxes a touch
    if (house_income < last_house_income || buis_profit < 0.0f)
        delta -= 0.05f;

    // Monetary tightness: if rates are already high, lean slightly looser fiscally
    if (interest_rate > 5.0f)
        delta -= 0.05f;

    // Gentle mean reversion toward policy anchor
    delta += (tax_target - tax_rate) * 0.01f;

    // Apply + clamp
    tax_rate += delta;
    if (tax_rate < 0.0f)  tax_rate = 0.0f;
    if (tax_rate > 60.0f) tax_rate = 60.0f;
}





};