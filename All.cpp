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
        if(inflation > 1 && inflation < 4)
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


    }





    void changeInflation()
    {
        
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



};