#include "Regression.h"
#define ARMA_DONT_PRINT_ERRORS
#include <armadillo>
#include <boost/math/distributions.hpp>
#include "Counter.h"
#include "Product.h"
#include "IndexedMap.h"

// I should really spawn this into a LinearModel class
struct RegResults {
    bool ok;
    arma::mat C;
    arma::colvec b;
    arma::colvec sol;
    arma::colvec res;
    arma::colvec serr;
    arma::colvec tval;
    arma::colvec pval;

    double fval;
    double fpval;
    double rsq;
    double rsqadj;
    double loglik;
    double aic;
    double bic;

    bool solve();
};

static arma::colvec cdf(const arma::colvec &x, uint32_t ndof) {
    unsigned nrows = x.n_rows;
    arma::colvec y(nrows);
    boost::math::students_t st(ndof);
    for (unsigned j = 0; j < nrows; ++j) {
        y(j) = cdf(st, x(j));
    }
    return y;
}

bool RegResults::solve() {
    // Dimensionality of the problem
    uint32_t nobs = C.n_rows;
    uint32_t ncoef = C.n_cols;

    // Initialize all metrics to NAN
    rsq = rsqadj = fval = fpval = loglik = aic = bic =
        std::numeric_limits<double>::quiet_NaN();

    // Check dimensions
    if (nobs <= ncoef) return false;

    // Solve system with LSQ
    ok = arma::solve(sol, C, b);
    if (not ok) return false;

    try {
        // Residuals
        res = b - C * sol;

        // Degrees of freedom
        uint32_t ndof = nobs - ncoef;

        // Variance of residuals
        double s2 = arma::dot(res, res) / ndof;

        // Standard errors
        serr = arma::sqrt(s2 * arma::diagvec(arma::pinv(C.t() * C)));

        // t-values and respective p-values
        tval = sol / serr;
        pval = (1 - cdf(arma::abs(tval), ndof)) * 2;

        // R-squared measures
        rsq = 1 - arma::dot(res, res) / arma::dot(b, b);
        rsqadj = 1 - (1 - rsq) * (double(nobs - 1) / (nobs - ncoef));

        // Compute F-value and respective probability for model selection
        fval = (rsq / (ncoef - 1)) / ((1 - rsq) / ndof);
        boost::math::fisher_f ff(ncoef - 1, nobs - ncoef);
        fpval = 1 - cdf(ff, fval);

        // log likelihood for model selection with Akaike information coefficients
        loglik = -(nobs * 0.5) * (1 + log(2 * M_PI)) -
                 (nobs / 2.) * log(arma::dot(res, res) / nobs);
        aic = -(2. * loglik) / nobs + double(2 * ncoef) / nobs;
        bic = -(2. * loglik) / nobs + double(ncoef * log(nobs)) / nobs;
    } catch (...) {
        // bad luck
        return false;
    }
    return true;
}

void summary(const Snapshot::EventMap &events, const std::string &header,
             const std::string &dependent_name, std::ostream &out) {
    struct Option {
        std::string name;
        std::function<double(size_t)> convert;
    };
    std::vector<Option> options = {
        {"N", [](size_t n) { return n; }},
        {"logN", [](size_t n) { return log(n) / log(10); }},
        {"N2", [](size_t n) { return n * n; }},
        {"NlogN", [](size_t n) { return n * log(n) / log(10); }}};

    for (const auto &ism : events) {
        std::string event_name = ism.first;
        const Snapshot::Event &event(ism.second);

        size_t numsamples = event.N.size();
        size_t numvars = event.metrics.size();

        // find the dependent index
        size_t dependent_index;
        bool found_index = false;
        for (int col = 0; col < numvars; ++col) {
            std::string metric_name = event.metrics[col].name;
            if (metric_name == dependent_name) {
                dependent_index = col;
                found_index = true;
                break;
            }
        }
        if (!found_index) {
            out << "Could not find dependent variable [" << dependent_name
                << "] in the metrics list\n";
            return;
        }

        // Build the stats matrix
        RegResults reg;
        reg.C = arma::mat(numsamples, numvars);
        reg.b = arma::colvec(numsamples);

        // cycle through all samples, filling up the matrix
        // we will replace later the dependent variable column with N/logN/etc
        for (size_t col = 0; col < numvars; ++col) {
            for (int row = 0; row < numsamples; ++row) {
                reg.C(row, col) = event.metrics[col].values[row];
            }
        }
        for (int row = 0; row < numsamples; ++row) {
            reg.b(row) = event.metrics[dependent_index].values[row];
        }

        RegResults bestreg;
        std::string bestname;
        bool found = false;

        for (const auto &option : options) {
            // Replace dependent variable column with flavors of "N"
            for (size_t row = 0; row < numsamples; ++row) {
                reg.C(row, dependent_index) = option.convert(event.N[row]);
            }
            if (!reg.solve()) continue;
            // if (reg.pval.max() > 0.05) continue;
            if ((not found) or (reg.aic < bestreg.aic)) {
                bestreg = reg;
                bestname = option.name;
                found = true;
            }
        }

        if (not found) {
            out << "    Model did not converge or not enough points\n";
        } else {
            out << "\n========== Best Model:\n" << event.name << ", ";
            char line[256];
            snprintf(line, sizeof(line), " Rsq:%5.2f F:%f LL:%f aic:%f bic:%f \n",
                     bestreg.rsq, bestreg.fpval, bestreg.loglik, bestreg.aic,
                     bestreg.bic);
            out << header << "," << line;
            for (size_t col = 0; col < numvars; ++col) {
                std::string colname = event.metrics[col].name;
                if (col == dependent_index) colname = bestname;
                snprintf(line, sizeof(line), "   %-15s  p:%7.5f coef:%g\n",
                         colname.c_str(), bestreg.pval(col), bestreg.sol(col));
                out << line;
            }
            out << "\n";
        }
    }
}
