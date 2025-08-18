#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <iomanip>
#include <algorithm>

// --------- helpers ----------
template <typename T>
T clampv(T v, T lo, T hi) { return (v < lo) ? lo : (v > hi) ? hi : v; }

// ===========================
// Parameters & Exogenous
// ===========================
struct Params {
    int    T = 60;            // periods
    // Demand/behavior
    double c0 = 20.0;         // autonomous consumption
    double c1 = 0.6;          // MPC
    double s_rate = 0.10;     // savings rate from adj. disp. income
    double wageShare = 0.65;  // baseline wage share
    // Investment & real dynamics
    double I_exo = 30.0;      // base investment (flow)
    double lambda_adj = 0.30; // speed of output adjustment (0..1)
    double deltaK = 0.05;     // capital depreciation
    double A_prod = 0.8;      // productivity scale in potential output
    double alphaK = 0.30;     // elasticity of output wrt capital in Y_pot
    // Fiscal policy
    double t_h = 0.15;        // wage tax
    double t_f = 0.20;        // profit tax
    double G_F = 25.0;        // gov purchases from firms
    double G_H = 15.0;        // gov wages/transfers to households
    // Monetary policy baseline + spreads
    double r_policy = 0.03;
    double spread_L = 0.04;   // loan rate = r_policy + spread_L
    double spread_D = 0.01;   // deposit rate = max(r_policy - spread_D, 0)
    // Household credit
    double L_H_new = 6.0;     // new HH borrowing each period (flow)
    double rho_H = 0.20;      // HH loan amortization rate per period
    // Firm credit (stocks)
    double rho_F = 0.10;      // firm loan amortization rate per period
    // Initial stocks
    double K0 = 500.0;        // initial capital
    double Lf0 = 200.0;       // initial firm loans
    double Lh0 = 50.0;        // initial household loans
    double D0  = 150.0;       // initial deposits
};

struct Exogenous {
    std::vector<double> foreignInv;     // adds to investment flow
    std::vector<double> strikeSeverity; // 0..1 (reduces effective labor/wage share)
    std::vector<double> policyDelta;    // delta to r_policy (can be 0)
    explicit Exogenous(int T)
      : foreignInv(T,0.0), strikeSeverity(T,0.0), policyDelta(T,0.0) {}
    void addForeignInvestmentPulse(int t, double amt){ if(t>=1 && t<= (int)foreignInv.size()) foreignInv[t-1]+=amt; }
    void addStrikePulse(int t, double sev){ if(t>=1 && t<= (int)strikeSeverity.size()) strikeSeverity[t-1]=clampv(sev,0.0,1.0); }
    void addPolicyShock(int t, double dr){ if(t>=1 && t<= (int)policyDelta.size()) policyDelta[t-1]+=dr; }
};

// ===========================
// Agents (stateless “policies” here)
// ===========================
struct Government {
    double t_h, t_f, G_F, G_H;
    Government(double th,double tf,double gF,double gH):t_h(th),t_f(tf),G_F(gF),G_H(gH){}
    double taxesH(double W)  const { return t_h * W; }
    double taxesF(double Pi) const { return t_f * Pi; }
};

struct Banks {
    double r_policy, sL, sD;
    Banks(double rp,double spreadL,double spreadD):r_policy(rp),sL(spreadL),sD(spreadD){}
    void rates(double dPol, double& r_p,double& r_L,double& r_D) const {
        r_p = r_policy + dPol;
        r_L = r_p + sL;
        r_D = std::max(r_p - sD, 0.0);
    }
};

// ===========================
// Series container
// ===========================
struct Series {
    std::vector<int> t;
    std::vector<double> Y, Ystar, Ypot, C, I, G_F, G_H, W, Pi_f, Taxes_H, Taxes_F;
    std::vector<double> Yd_base, S, Int_D, Int_LH, Int_LF, Pi_b, GovDef;
    std::vector<double> r_p, r_L, r_D, w_eff;
    std::vector<double> K, LoansF, LoansH, Deposits;

    void reserve(int T){
        auto res=[&](auto& v){ v.reserve(T); };
        res(t);res(Y);res(Ystar);res(Ypot);res(C);res(I);res(G_F);res(G_H);res(W);res(Pi_f);
        res(Taxes_H);res(Taxes_F);res(Yd_base);res(S);res(Int_D);res(Int_LH);res(Int_LF);res(Pi_b);
        res(GovDef);res(r_p);res(r_L);res(r_D);res(w_eff);res(K);res(LoansF);res(LoansH);res(Deposits);
    }
};

// ===========================
// Economy engine (dynamic)
// ===========================
class EconomyModel {
public:
    EconomyModel(Params p, Exogenous exo) : P(p), EX(exo), G(p.t_h,p.t_f,p.G_F,p.G_H), B(p.r_policy,p.spread_L,p.spread_D)
    {
        // init stocks and last-period output for inertia
        K = P.K0; Lf = P.Lf0; Lh = P.Lh0; D = P.D0;
        Y_last = P.A_prod * std::pow(std::max(K,0.0), P.alphaK); // start near potential
    }

    Series run(const std::string& csv="timeseries.csv", bool printPreview=true){
        Series Srs; Srs.reserve(P.T);
        std::ofstream fout(csv);
        fout << "period,Y,Y_star,Y_potential,C,I,G_F,G_H,W,Pi_f,Taxes_H,Taxes_F,"
                "Yd_base,S,Int_D,Int_LH,Int_LF,BankProfits,GovDeficit,r_p,r_L,r_D,"
                "w_eff,K,LoansF,LoansH,Deposits\n";

        for(int k=0;k<P.T;++k){
            int period=k+1;

            // policy & rates
            double r_p_t,r_L_t,r_D_t;
            B.rates(EX.policyDelta[k], r_p_t, r_L_t, r_D_t);

            // exogenous: FI, strike
            double FI = EX.foreignInv[k];
            double strike = clampv(EX.strikeSeverity[k],0.0,1.0);
            double w_eff_t = P.wageShare * (1.0 - strike);

            // Investment flow & potential output (from capital)
            double I_t = P.I_exo + FI;
            double Y_pot = P.A_prod * std::pow(std::max(K,0.0), P.alphaK);

            // ---- Static demand target Y* (as before, but using w_eff_t) ----
            // We still approximate deposit interest on a flow basis to compute the *target*.
            // Then we apply inertia towards that target.
            // Int_LH* uses **loan stock** now: r_L * Lh
            double Int_LH_star = r_L_t * Lh;
            double A = (1.0 - P.t_h) * w_eff_t * (1.0 + r_D_t * P.s_rate);
            double Bterm = P.G_H + r_D_t * P.s_rate * (P.G_H - Int_LH_star) - Int_LH_star;
            double denom = 1.0 - P.c1 * A;
            double RHS   = P.c0 + P.c1 * Bterm + I_t + P.G_F;
            double Y_star = (denom!=0.0)? (RHS/denom) : Y_last;

            // Optional supply cap (won't bind by default): cap demand by potential
            double Y_cap = Y_star; // set to std::min(Y_star, Y_pot) if you want hard capacity
            // Partial adjustment: dynamics!
            double Y_t = Y_last + P.lambda_adj * (Y_cap - Y_last);

            // Flows derived from **actual** Y_t
            double W_t = w_eff_t * Y_t;
            double Pi_f_t = Y_t - W_t;
            double Taxes_H_t = G.taxesH(W_t);
            double Taxes_F_t = G.taxesF(Pi_f_t);

            double Yd_base_t = (W_t - Taxes_H_t) + P.G_H;

            // Household credit: new borrowing & amortization on stock
            double repay_H = P.rho_H * Lh;
            Lh = std::max(0.0, Lh + P.L_H_new - repay_H);
            double Int_LH_t = r_L_t * Lh; // interest on HH **stock**

            // Deposits: S flow adds to deposit stock; interest paid on stock
            double S_flow = P.s_rate * (Yd_base_t - Int_LH_t); // exclude Int_D for stability
            double Int_D_t = r_D_t * D;                        // interest on **deposit stock**
            double C_t = P.c0 + P.c1 * ((Yd_base_t + Int_D_t) - Int_LH_t);

            // Firm loans: new lending I_t, amortization on stock; interest on stock
            double repay_F = P.rho_F * Lf;
            Lf = std::max(0.0, Lf + I_t - repay_F);
            double Int_LF_t = r_L_t * Lf;

            // Update deposit stock after flows
            D = std::max(0.0, D + S_flow - P.L_H_new + repay_H); // simplistic deposit dynamics

            // Government balance (no debt stock here)
            double GovDef = (P.G_H + P.G_F) - (Taxes_H_t + Taxes_F_t);

            // Bank profits: interest received on loans - interest paid on deposits
            double Pi_b_t = (Int_LF_t + Int_LH_t) - Int_D_t;

            // Capital accumulation
            K = std::max(0.0, (1.0 - P.deltaK)*K + I_t);

            // Save & print
            Srs.t.push_back(period);
            Srs.Y.push_back(Y_t); Srs.Ystar.push_back(Y_star); Srs.Ypot.push_back(Y_pot);
            Srs.C.push_back(C_t); Srs.I.push_back(I_t);
            Srs.G_F.push_back(P.G_F); Srs.G_H.push_back(P.G_H);
            Srs.W.push_back(W_t); Srs.Pi_f.push_back(Pi_f_t);
            Srs.Taxes_H.push_back(Taxes_H_t); Srs.Taxes_F.push_back(Taxes_F_t);
            Srs.Yd_base.push_back(Yd_base_t); Srs.S.push_back(S_flow);
            Srs.Int_D.push_back(Int_D_t); Srs.Int_LH.push_back(Int_LH_t); Srs.Int_LF.push_back(Int_LF_t);
            Srs.Pi_b.push_back(Pi_b_t); Srs.GovDef.push_back(GovDef);
            Srs.r_p.push_back(r_p_t); Srs.r_L.push_back(r_L_t); Srs.r_D.push_back(r_D_t);
            Srs.w_eff.push_back(w_eff_t); Srs.K.push_back(K); Srs.LoansF.push_back(Lf);
            Srs.LoansH.push_back(Lh); Srs.Deposits.push_back(D);

            fout << period << ","
                 << Y_t << "," << Y_star << "," << Y_pot << ","
                 << C_t << "," << I_t << "," << P.G_F << "," << P.G_H << ","
                 << W_t << "," << Pi_f_t << "," << Taxes_H_t << "," << Taxes_F_t << ","
                 << Yd_base_t << "," << S_flow << "," << Int_D_t << "," << Int_LH_t << ","
                 << Int_LF_t << "," << Pi_b_t << "," << GovDef << ","
                 << r_p_t << "," << r_L_t << "," << r_D_t << ","
                 << w_eff_t << "," << K << "," << Lf << "," << Lh << "," << D << "\n";

            Y_last = Y_t; // carry forward for dynamics
        }
        fout.close();

        if (printPreview) {
            std::cout << std::fixed << std::setprecision(2);
            std::cout << "period   Y     C     I    K     Lf    Lh    D    r_p   r_L   r_D   w_eff\n";
            for (size_t i=0;i<Srs.t.size();++i){
                if (i<12 || i+3>=Srs.t.size()){
                    std::cout << std::setw(6) << Srs.t[i] << " "
                              << std::setw(6) << Srs.Y[i] << " "
                              << std::setw(6) << Srs.C[i] << " "
                              << std::setw(6) << Srs.I[i] << " "
                              << std::setw(6) << Srs.K[i] << " "
                              << std::setw(6) << Srs.LoansF[i] << " "
                              << std::setw(6) << Srs.LoansH[i] << " "
                              << std::setw(6) << Srs.Deposits[i] << " "
                              << std::setw(5) << Srs.r_p[i] << " "
                              << std::setw(5) << Srs.r_L[i] << " "
                              << std::setw(5) << Srs.r_D[i] << " "
                              << std::setw(6) << Srs.w_eff[i] << "\n";
                }
            }
            std::cout << "... wrote timeseries.csv\n";
        }
        return Srs;
    }

private:
    Params P;
    Exogenous EX;
    Government G;
    Banks B;

    // state stocks
    double K, Lf, Lh, D;
    double Y_last;
};

// ===========================
// Example main()
// ===========================
int main(){
    Params P;            // tweak if you like
    Exogenous EX(P.T);   // paths you can edit

    // Example dynamics you will SEE over time:
    EX.addForeignInvestmentPulse(15, 25.0);  // FI shock to I
    EX.addStrikePulse(18, 0.40);             // strike (40%) hits wages/labor
    EX.addPolicyShock(30, -0.015);           // rate cut of 1.5%

    EconomyModel model(P, EX);
    model.run("timeseries.csv", true);
    return 0;
}
