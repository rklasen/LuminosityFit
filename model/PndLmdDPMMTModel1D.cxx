/*
 * PndLmdSignalModel.cxx
 *
 *  Created on: Dec 19, 2012
 *      Author: steve
 */

#include "PndLmdDPMMTModel1D.h"

#include <cmath>
#include <map>
#include <limits>

#include "TMath.h"
#include "TDatabasePDG.h"

PndLmdDPMMTModel1D::PndLmdDPMMTModel1D(std::string name_,
    LumiFit::DPMElasticParts elastic_type_) :
    Model1D(name_) {

  elastic_type = elastic_type_;
  if (elastic_type == LumiFit::COUL) {
    model_func = &PndLmdDPMMTModel1D::getRawCoulombPart;
  } else if (elastic_type == LumiFit::INT) {
    model_func = &PndLmdDPMMTModel1D::getRawInterferencePart;
  } else if (elastic_type == LumiFit::HAD) {
    model_func = &PndLmdDPMMTModel1D::getRawHadronicPart;
  } else if (elastic_type == LumiFit::HAD_RHO_B_SIGTOT) {
    model_func = &PndLmdDPMMTModel1D::getRawRhoBSigtotHadronicPart;
  } else if (elastic_type == LumiFit::ALL_RHO_B_SIGTOT) {
    model_func = &PndLmdDPMMTModel1D::getRawRhoBSigtotFullElastic;
  } else {
    model_func = &PndLmdDPMMTModel1D::getRawFullElastic;
  }

  // here a bunch of parameters, which are constants, will be set
  init();
  initModelParameters();
}

PndLmdDPMMTModel1D::~PndLmdDPMMTModel1D() {
}

void PndLmdDPMMTModel1D::updateDomainFromPars(double *par) {
}

void PndLmdDPMMTModel1D::init() {
  TDatabasePDG *pdg = TDatabasePDG::Instance();
  M = pdg->GetParticle(-2212)->Mass();
  pi = TMath::Pi();
  hbarc2 = 0.389379;
  alpha = 1. / 137.036;

  alpha_squared_4pi = 4.0 * pi * alpha * alpha;
  one_over_16pi_hbarc2 = 1.0 / (16.0 * pi * hbarc2);
}

void PndLmdDPMMTModel1D::initModelParameters() {
  //strictly fixed parameters
  p_lab = getModelParameterSet().addModelParameter("p_lab");
  p_lab->setSuperior(true);
  luminosity = getModelParameterSet().addModelParameter("luminosity");
  luminosity->setSuperior(true);
  E_lab = getModelParameterSet().addModelParameter("E_lab");
  S = getModelParameterSet().addModelParameter("S");
  pcm2 = getModelParameterSet().addModelParameter("pcm2");
  gamma = getModelParameterSet().addModelParameter("gamma");
  beta = getModelParameterSet().addModelParameter("beta");
  beta_lab_cms = getModelParameterSet().addModelParameter("beta_lab_cms");
  //possibly free fit parameters
  sigma_tot = getModelParameterSet().addModelParameter("sigma_tot");
  b = getModelParameterSet().addModelParameter("b");
  rho = getModelParameterSet().addModelParameter("rho");
  A1 = getModelParameterSet().addModelParameter("A1");
  A2 = getModelParameterSet().addModelParameter("A2");
  A3 = getModelParameterSet().addModelParameter("A3");
  T1 = getModelParameterSet().addModelParameter("T1");
  T2 = getModelParameterSet().addModelParameter("T2");
}

double PndLmdDPMMTModel1D::getDelta(const double t) const {
  return 1.408450704 * TMath::Abs(t); //TMath::Abs(t) / 0.71; division costs more
}

double PndLmdDPMMTModel1D::getProtonDipoleFormFactor(const double t) const {
  return std::pow((1.0 + getDelta(t)), -2);
}

double PndLmdDPMMTModel1D::getRawCoulombPart(const double *x) const {
  double p1 = alpha_squared_4pi * std::pow(getProtonDipoleFormFactor(x[0]), 4)
      * hbarc2 / std::pow(beta->getValue() * x[0], 2); //Coulomb part
  return p1;
}

double PndLmdDPMMTModel1D::getRawInterferencePart(const double *x) const {
  // we need the next line because if user wants to fit t spectrum it is plotted for positive t
  // so x[0] will contain positive t number
  // however in case of theta fitting the t is automatically calculated to be negative
  // to make them both work this step is necessary
  // (ok maybe for later its better to always give positive t here...)
  double t = -std::fabs(x[0]);
  double del = getDelta(t);

  double dd2 = 4.0 * del, dd1 = 0.5 * b->getValue() * std::fabs(t) + dd2;
  double logdd1 = std::log(dd1), logdd2 = std::log(dd2);
  double delta = alpha * (0.577 + logdd1 + dd2 * logdd2 + 2.0 * del);

  double int_part = alpha * sigma_tot->getValue()
      * std::pow(getProtonDipoleFormFactor(t), 2)
      * std::exp(0.5 * b->getValue() * t)
      * (rho->getValue() * cos(delta) + sin(delta))
      / (beta->getValue() * std::fabs(t)); //Exact version as dpm states

  return int_part;
}

double PndLmdDPMMTModel1D::getRawHadronicPart(const double *x) const {
  double t = -std::fabs(x[0]);

  double t_over_T2(t / T2->getValue());
  double had_part = A1->getValue()
      * std::pow(
          std::exp(t / (2.0 * T1->getValue()))
              - A2->getValue() * std::exp(0.5 * t_over_T2), 2)
      + A3->getValue() * std::exp(t_over_T2);

  return had_part;
}

double PndLmdDPMMTModel1D::getRawRhoBSigtotHadronicPart(const double *x) const {
  double t = -std::fabs(x[0]);

  double had_part = std::pow(sigma_tot->getValue(), 2)
      * (1.0 + std::pow(rho->getValue(), 2)) * std::exp(b->getValue() * t)
      * one_over_16pi_hbarc2;

  return had_part;
}

double PndLmdDPMMTModel1D::getRawFullElastic(const double *x) const {
  return (getRawCoulombPart(x) + getRawInterferencePart(x)
      + getRawHadronicPart(x));
}

double PndLmdDPMMTModel1D::getRawRhoBSigtotFullElastic(const double *x) const {
  return (getRawCoulombPart(x) + getRawInterferencePart(x)
      + getRawRhoBSigtotHadronicPart(x));
}

mydouble PndLmdDPMMTModel1D::eval(const double *x) const {
  return luminosity->getValue() * (this->*model_func)(x);
}

void PndLmdDPMMTModel1D::updateDomain() {
  setDomain(0, std::numeric_limits<double>::max());
}

double PndLmdDPMMTModel1D::getRho() const {
  return rho->getValue();
}

double PndLmdDPMMTModel1D::getB() const {
  return b->getValue();
}

double PndLmdDPMMTModel1D::getSigmaTotal() const {
  return sigma_tot->getValue();
}
